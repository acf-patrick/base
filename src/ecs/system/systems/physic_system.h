#pragma once

#ifdef USE_PHYSIC_SYSTEM

#include <box2d/box2d.h>

#include "../../../application/application.h"
#include "../../../resource/resource.h"
#include "../../../resource/resources/physic_world.h"
#include "../system.h"

#define WORLD_SCALE 80.0f
#define MtoPX(x) x* WORLD_SCALE
#define PXtoM(x) x / WORLD_SCALE

namespace entix {
namespace ecs {
namespace system {

class PhysicSystem : public entix::ecs::ISystem {
    core::EventListner eventListner;
    b2WorldId _worldId = b2_nullWorldId;

   public:
    PhysicSystem() : ISystem("PhysicSystem") {
        auto resources = ResourceManager::Get();
        resources->add<resource::PhysicWorld>((b2Vec2){0.0, 10.0});
        _worldId = resources->read<resource::PhysicWorld>()->worldId;
    }

    ~PhysicSystem() { b2DestroyWorld(_worldId); }

    bool run() override {
        if (b2World_IsValid(_worldId)) {
            b2World_Step(_worldId,
                         1.0 / core::Application::Get().getPreferredFramerate(),
                         6);
            return true;
        }
        return false;
    }
};


}  // namespace system
}  // namespace ecs
}  // namespace entix

#endif