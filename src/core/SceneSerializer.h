#pragma once

#include "Serializer.h"
#include "../ecs/ECSCoordinator.h"
#include "../ecs/Components.h"
#include <string>

namespace engine {
namespace core {

/// SceneSerializer — Guarda/carga escenas ECS a/desde archivos JSON.
///
/// Formato:
/// {
///   "scene": "MiNivel",
///   "entities": [
///     {
///       "tags": 1,
///       "transform": { "x": 100, "y": 200, "rotation": 0, "scaleX": 1, "scaleY": 1 },
///       "physics": { "mass": 1.5, "drag": 0.5, "restitution": 0.15, "isStatic": false },
///       "collider": { "w": 28, "h": 28, "isStatic": false },
///       "sprite": { "r": 0, "g": 255, "b": 0, "a": 255, "w": 28, "h": 28 }
///     }
///   ]
/// }
///
class SceneSerializer {
public:
    /// Guardar todas las entidades a un archivo JSON
    static bool saveScene(ecs::ECSCoordinator& ecs, const std::string& path,
                          const std::string& sceneName = "Untitled") {
        JsonWriter w;
        w.beginObject();
        w.keyValue("scene", sceneName);

        w.beginArray("entities");

        // Iterar sobre todas las entidades con TransformComponent
        auto& transforms = ecs.getStorage<ecs::TransformComponent>();
        for (uint32_t i = 0; i < transforms.size(); i++) {
            ecs::Entity entity = transforms.getEntity(i);
            if (!ecs.isAlive(entity)) continue;

            w.beginObject();

            // Tags
            w.keyValue("tags", static_cast<int>(ecs.getTagMask(entity).to_ulong()));

            // Transform
            auto& tf = transforms.getDense(i);
            w.key("transform");
            w.beginObject();
            w.keyValue("x", tf.transform.position.x);
            w.keyValue("y", tf.transform.position.y);
            w.keyValue("rotation", tf.transform.rotation);
            w.keyValue("scaleX", tf.transform.scale.x);
            w.keyValue("scaleY", tf.transform.scale.y);
            w.endObject();

            // Physics
            if (ecs.hasComponent<ecs::PhysicsComponent>(entity)) {
                auto& phys = ecs.getComponent<ecs::PhysicsComponent>(entity);
                w.key("physics");
                w.beginObject();
                w.keyValue("mass", phys.mass);
                w.keyValue("drag", phys.drag);
                w.keyValue("restitution", phys.restitution);
                w.keyValue("friction", phys.friction);
                w.keyValue("isStatic", phys.isStatic);
                w.endObject();
            }

            // Collider
            if (ecs.hasComponent<ecs::ColliderComponent>(entity)) {
                auto& col = ecs.getComponent<ecs::ColliderComponent>(entity);
                math::Vector2D size = col.aabb.halfSize() * 2.0f;
                w.key("collider");
                w.beginObject();
                w.keyValue("w", size.x);
                w.keyValue("h", size.y);
                w.keyValue("isStatic", col.isStatic);
                w.endObject();
            }

            // Sprite
            if (ecs.hasComponent<ecs::SpriteComponent>(entity)) {
                auto& spr = ecs.getComponent<ecs::SpriteComponent>(entity);
                w.key("sprite");
                w.beginObject();
                w.keyValue("r", static_cast<int>(spr.color.r));
                w.keyValue("g", static_cast<int>(spr.color.g));
                w.keyValue("b", static_cast<int>(spr.color.b));
                w.keyValue("a", static_cast<int>(spr.color.a));
                w.keyValue("w", spr.size.x);
                w.keyValue("h", spr.size.y);
                w.endObject();
            }

            w.endObject();
        }

        w.endArray();
        w.endObject();

        return w.saveToFile(path);
    }

    /// Cargar entidades desde un archivo JSON
    static int loadScene(ecs::ECSCoordinator& ecs, const std::string& path) {
        JsonReader r;
        if (!r.loadFromFile(path)) return -1;

        int loaded = 0;

        if (!r.expectChar('{')) return -1;

        // Read top-level keys
        while (!r.peekChar('}')) {
            std::string key = r.readKey();

            if (key == "scene") {
                r.readString();  // scene name, skip
            }
            else if (key == "entities") {
                if (!r.expectChar('[')) return -1;

                while (!r.peekChar(']')) {
                    if (loadEntity(ecs, r)) loaded++;
                    r.skipComma();
                }

                r.expectChar(']');
            }
            else {
                r.skipValue();
            }
            r.skipComma();
        }

        r.expectChar('}');
        return loaded;
    }

private:
    static bool loadEntity(ecs::ECSCoordinator& ecs, JsonReader& r) {
        if (!r.expectChar('{')) return false;

        ecs::Entity entity = ecs.createEntity();
        uint16_t tags = 0;

        math::Vector2D pos;
        float rotation = 0, scaleX = 1, scaleY = 1;
        bool hasTransform = false;

        while (!r.peekChar('}')) {
            std::string key = r.readKey();

            if (key == "tags") {
                tags = static_cast<uint16_t>(r.readInt());
            }
            else if (key == "transform") {
                hasTransform = true;
                r.expectChar('{');
                while (!r.peekChar('}')) {
                    std::string tk = r.readKey();
                    if (tk == "x") pos.x = r.readFloat();
                    else if (tk == "y") pos.y = r.readFloat();
                    else if (tk == "rotation") rotation = r.readFloat();
                    else if (tk == "scaleX") scaleX = r.readFloat();
                    else if (tk == "scaleY") scaleY = r.readFloat();
                    else r.skipValue();
                    r.skipComma();
                }
                r.expectChar('}');
            }
            else if (key == "physics") {
                float mass = 1.0f, drag = 0.0f, rest = 0.0f, fric = 0.3f;
                bool isStatic = false;
                r.expectChar('{');
                while (!r.peekChar('}')) {
                    std::string pk = r.readKey();
                    if (pk == "mass") mass = r.readFloat();
                    else if (pk == "drag") drag = r.readFloat();
                    else if (pk == "restitution") rest = r.readFloat();
                    else if (pk == "friction") fric = r.readFloat();
                    else if (pk == "isStatic") isStatic = r.readBool();
                    else r.skipValue();
                    r.skipComma();
                }
                r.expectChar('}');

                ecs::PhysicsComponent phys(mass);
                phys.drag = drag;
                phys.restitution = rest;
                phys.friction = fric;
                phys.isStatic = isStatic;
                ecs.addComponent<ecs::PhysicsComponent>(entity, phys);
            }
            else if (key == "collider") {
                float w = 0, h = 0;
                bool isStatic = false;
                r.expectChar('{');
                while (!r.peekChar('}')) {
                    std::string ck = r.readKey();
                    if (ck == "w") w = r.readFloat();
                    else if (ck == "h") h = r.readFloat();
                    else if (ck == "isStatic") isStatic = r.readBool();
                    else r.skipValue();
                    r.skipComma();
                }
                r.expectChar('}');

                ecs.addComponent<ecs::ColliderComponent>(entity,
                    ecs::ColliderComponent(math::Vector2D(w, h), isStatic));
            }
            else if (key == "sprite") {
                int cr = 255, cg = 255, cb = 255, ca = 255;
                float w = 0, h = 0;
                r.expectChar('{');
                while (!r.peekChar('}')) {
                    std::string sk = r.readKey();
                    if (sk == "r") cr = r.readInt();
                    else if (sk == "g") cg = r.readInt();
                    else if (sk == "b") cb = r.readInt();
                    else if (sk == "a") ca = r.readInt();
                    else if (sk == "w") w = r.readFloat();
                    else if (sk == "h") h = r.readFloat();
                    else r.skipValue();
                    r.skipComma();
                }
                r.expectChar('}');

                ecs.addComponent<ecs::SpriteComponent>(entity,
                    ecs::SpriteComponent(
                        math::Color(static_cast<uint8_t>(cr), static_cast<uint8_t>(cg),
                                    static_cast<uint8_t>(cb), static_cast<uint8_t>(ca)),
                        w, h));
            }
            else {
                r.skipValue();
            }
            r.skipComma();
        }

        r.expectChar('}');

        if (hasTransform) {
            ecs::TransformComponent tf(pos);
            tf.transform.rotation = rotation;
            tf.transform.scale = {scaleX, scaleY};
            ecs.addComponent<ecs::TransformComponent>(entity, tf);
        }

        // Set tags
        if (tags & 1) ecs.setTag(entity, ecs::TAG_PLAYER);
        if (tags & 2) ecs.setTag(entity, ecs::TAG_PLATFORM);

        return true;
    }
};

} // namespace core
} // namespace engine
