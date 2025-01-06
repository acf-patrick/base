#pragma once

#ifdef USE_PHYSIC_SYSTEM

#include <box2d/box2d.h>

#include "../../logger/logger.h"

namespace entix {
namespace resource {

struct PhysicWorld {
    b2WorldId worldId = b2_nullWorldId;
    b2Vec2 gravity;

    PhysicWorld(const b2Vec2& gravity) : gravity(gravity) {
        auto worldDef = b2DefaultWorldDef();
        worldDef.gravity = gravity;
        worldId = b2CreateWorld(&worldDef);

        if (!b2World_IsValid(worldId)) {
            Logger::error("Physic") << "Failed to create Physic world";
            Logger::endline();
        }
    }
};

}  // namespace resource
}  // namespace entix

#endif