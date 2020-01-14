#pragma once

#include <ECS/Components/BaseComponent.h>

#include <ECS/Components/ComponentTypes.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace cyd
{
struct TransformComponent final : BaseComponent
{
   TransformComponent() = default;
   COPIABLE( TransformComponent );
   virtual ~TransformComponent() = default;

   static constexpr ComponentType TYPE = ComponentType::TRANSFORM;

   bool init() override { return true; };
   void uninit() override{};

   glm::vec3 position = glm::vec3( 0.0f );
   glm::vec3 scaling  = glm::vec3( 1.0f );
   glm::quat rotation = glm::identity<glm::quat>();
};
}
