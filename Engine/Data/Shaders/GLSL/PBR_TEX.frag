#version 450
#extension GL_ARB_separate_shader_objects : enable

// View and environment
// =================================================================================================
layout( set = 0, binding = 1 ) uniform DirectionalLights
{
   bool enabled;
   vec4 direction;
   vec4 color;
}
dirLights;

// Material properties
// =================================================================================================
layout( set = 1, binding = 0 ) uniform sampler2D albedo;
layout( set = 1, binding = 1 ) uniform sampler2D normalMap;
layout( set = 1, binding = 2 ) uniform sampler2D metallicMap;
layout( set = 1, binding = 3 ) uniform sampler2D roughnessMap;
layout( set = 1, binding = 4 ) uniform sampler2D aoMap;

layout( location = 0 ) in vec3 inTexCoord;
layout( location = 1 ) in vec3 inNormal;
layout( location = 2 ) in vec3 fragPos;
layout( location = 3 ) in vec3 viewPos;

layout( location = 0 ) out vec4 outColor;

const float PI = 3.141592653589793;

// Formulas
// =================================================================================================
float DistributionGGX( vec3 N, vec3 H, float roughness )
{
   float a      = roughness * roughness;
   float a2     = a * a;
   float NdotH  = max( dot( N, H ), 0.0 );
   float NdotH2 = NdotH * NdotH;

   float nom   = a2;
   float denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0 );
   denom       = PI * denom * denom;

   return nom / denom;
}

float GeometrySchlickGGX( float NdotV, float roughness )
{
   float r = ( roughness + 1.0 );
   float k = ( r * r ) / 8.0;

   float nom   = NdotV;
   float denom = NdotV * ( 1.0 - k ) + k;

   return nom / denom;
}

float GeometrySmith( vec3 N, vec3 V, vec3 L, float roughness )
{
   float NdotV = max( dot( N, V ), 0.0 );
   float NdotL = max( dot( N, L ), 0.0 );
   float ggx2  = GeometrySchlickGGX( NdotV, roughness );
   float ggx1  = GeometrySchlickGGX( NdotL, roughness );

   return ggx1 * ggx2;
}

vec3 FresnelSchlick( float cosTheta, vec3 F0 )
{
   return F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );
}

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 Uncharted2Tonemap( vec3 col )
{
   return ( ( col * ( A * col + C * B ) + D * E ) / ( col * ( A * col + B ) + D * F ) ) - E / F;
}

// Helpers
// =================================================================================================
vec3 getNormalFromMap()
{
   vec3 tangentNormal = texture( normalMap, inTexCoord.xy ).xyz;  // * 2.0 - 1.0;

   vec3 Q1  = dFdx( fragPos );
   vec3 Q2  = dFdy( fragPos );
   vec2 st1 = dFdx( inTexCoord.xy );
   vec2 st2 = dFdy( inTexCoord.xy );

   vec3 N   = normalize( inNormal );
   vec3 T   = normalize( Q1 * st2.t - Q2 * st1.t );
   vec3 B   = -normalize( cross( N, T ) );
   mat3 TBN = mat3( T, B, N );

   return normalize( TBN * tangentNormal );
}

// =================================================================================================
void main()
{
   const vec3 albedo     = texture( albedo, inTexCoord.xy ).rgb;
   const float metallic  = texture( metallicMap, inTexCoord.xy ).r;
   const float roughness = texture( roughnessMap, inTexCoord.xy ).r;
   const float ao        = texture( aoMap, inTexCoord.xy ).r;

   const vec3 N = getNormalFromMap();
   const vec3 V = normalize( vec3( viewPos ) - fragPos );

   // Calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
   // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
   vec3 F0 = vec3( 0.04 );
   F0      = mix( F0, albedo, metallic );

   // Reflectance equation
   vec3 Lo = vec3( 0.0 );

   // Calculate per-light radiance
   // DIRECTIONAL LIGHT
   const vec3 L        = normalize( vec3( -dirLights.direction ) );
   const vec3 radiance = vec3( dirLights.color );

   // POINT LIGHT
   // const vec3 L            = normalize( vec3( lightPositions[i] ) - fragPos );
   // const float distance    = length( vec3( lightPositions[i] ) - fragPos );
   // const float attenuation = 1.0 / ( distance * distance );
   // const vec3 radiance     = vec3( lightColors[i] * attenuation );

   const vec3 H = normalize( V + L );

   // Cook-Torrance BRDF
   const float NDF = DistributionGGX( N, H, roughness );
   const float G   = GeometrySmith( N, V, L, roughness );
   const vec3 F    = FresnelSchlick( max( dot( H, V ), 0.0 ), F0 );

   const vec3 nominator    = NDF * G * F;
   const float denominator = 4 * max( dot( N, V ), 0.0 ) * max( dot( N, L ), 0.0 );
   const vec3 specular     = nominator / max( denominator, 0.0001 );

   // kS is equal to Fresnel
   const vec3 kS = F;

   // For energy conservation, the diffuse and specular light can't
   // be above 1.0 (unless the surface emits light); to preserve this
   // relationship the diffuse component (kD) should equal 1.0 - kS.
   vec3 kD = vec3( 1.0 ) - kS;

   // Multiply kD by the inverse metalness such that only non-metals
   // have diffuse lighting, or a linear blend if partly metal (pure metals
   // have no diffuse light).
   kD *= 1.0 - metallic;

   // Scale light by NdotL
   const float NdotL = max( dot( N, L ), 0.0 );

   // Add to outgoing radiance Lo
   Lo += int( dirLights.enabled ) * ( kD * ( albedo / PI ) + specular ) * radiance * NdotL;

   // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by
   // kS again   Lo += int( dirLight.enabled ) * calcLuminance( i, N, V, F0, albedo, metallic,
   // roughness );

   // Environment Lighting
   const vec3 ambient = vec3( 0.03 ) * albedo * ao;

   vec3 color = ambient + Lo;

   // HDR tonemapping
   const float exposureBias = 2.0;
   const vec3 whiteScale    = 1.0 / Uncharted2Tonemap( vec3( W ) );
   color                    = Uncharted2Tonemap( exposureBias * color ) * whiteScale;

   // Gamma correction
   color = pow( color, vec3( 1.0 / 2.2 ) );

   outColor = vec4( color, 1.0 );
}
