#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform MVP
{
	mat4 mv;
	mat4 proj;
} mvp;

layout(location = 0) out vec4 outColor;

void main()
{
    gl_Position = mvp.proj * mvp.mv * inPosition;
    outColor = inColor;
}