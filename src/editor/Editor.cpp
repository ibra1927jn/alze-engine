#include "Editor.h"
#include <cstdio>

namespace engine {
namespace editor {

// ════════════════════════════════════════════════════════════════
// Inicializacion
// ════════════════════════════════════════════════════════════════

void Editor::init(ecs::ECSCoordinator* ecs) {
    m_ecs = ecs;
    // Tema oscuro para el editor
    m_ui.theme.panelBg      = math::Color(20, 20, 32, 230);
    m_ui.theme.panelBorder  = math::Color(70, 70, 100);
    m_ui.theme.panelTitle   = math::Color(140, 200, 255);
    m_ui.theme.buttonNormal = math::Color(45, 45, 70);
    m_ui.theme.buttonHover  = math::Color(60, 60, 95);
    m_ui.theme.buttonActive = math::Color(80, 120, 180);
}

// ════════════════════════════════════════════════════════════════
// Input
// ════════════════════════════════════════════════════════════════

bool Editor::handleInput(const core::InputManager& input, float dt) {
    // F1 toggle — siempre activo
    if (input.isKeyPressed(SDL_SCANCODE_F1)) {
        toggle();
    }

    if (!m_active) return false;

    // Actualizar estado del mouse para UISystem
    auto mousePos = input.getMousePosition();
    m_mouseX = mousePos.x;
    m_mouseY = mousePos.y;
    m_mouseDown = input.isMouseButtonDown(SDL_BUTTON_LEFT);
    m_mousePressed = m_mouseDown && !m_prevMouseDown; // Detectar transicion
    m_prevMouseDown = m_mouseDown;

    // Gizmo: mover entidad seleccionada con flechas
    if (m_selectedEntity != ecs::NULL_ENTITY && m_ecs->isAlive(m_selectedEntity)) {
        if (m_ecs->hasComponent<ecs::Transform3DComponent>(m_selectedEntity)) {
            auto& transform = m_ecs->getComponent<ecs::Transform3DComponent>(m_selectedEntity);
            float speed = m_gizmoSpeed * dt;
            bool moved = false;

            // Flechas: X/Z en el plano horizontal
            if (input.isKeyDown(SDL_SCANCODE_RIGHT)) {
                transform.transform.position.x += speed;
                moved = true;
            }
            if (input.isKeyDown(SDL_SCANCODE_LEFT)) {
                transform.transform.position.x -= speed;
                moved = true;
            }
            if (input.isKeyDown(SDL_SCANCODE_UP)) {
                transform.transform.position.z -= speed;
                moved = true;
            }
            if (input.isKeyDown(SDL_SCANCODE_DOWN)) {
                transform.transform.position.z += speed;
                moved = true;
            }
            // Page Up/Down: Y (vertical)
            if (input.isKeyDown(SDL_SCANCODE_PAGEUP)) {
                transform.transform.position.y += speed;
                moved = true;
            }
            if (input.isKeyDown(SDL_SCANCODE_PAGEDOWN)) {
                transform.transform.position.y -= speed;
                moved = true;
            }

            if (moved) transform.dirty = true;
        }
    }

    // Scroll del panel de jerarquia con rueda (simulado con +/-)
    if (input.isKeyPressed(SDL_SCANCODE_EQUALS)) {
        m_hierarchyScroll = std::max(0, m_hierarchyScroll - 1);
    }
    if (input.isKeyPressed(SDL_SCANCODE_MINUS)) {
        int maxScroll = std::max(0, (int)m_entityList.size() - MAX_VISIBLE_ENTITIES);
        m_hierarchyScroll = std::min(maxScroll, m_hierarchyScroll + 1);
    }

    // Consumir input cuando el editor esta activo (evitar que el juego lo procese)
    return true;
}

// ════════════════════════════════════════════════════════════════
// Update
// ════════════════════════════════════════════════════════════════

void Editor::update(float dt) {
    if (!m_active || !m_ecs) return;

    // Refrescar lista de entidades vivas
    m_entityList.clear();
    auto sig = m_ecs->buildSignature<ecs::Transform3DComponent>();
    m_ecs->forEachEntity(sig, [this](ecs::Entity e) {
        m_entityList.push_back(e);
    });

    // Validar seleccion
    if (m_selectedEntity != ecs::NULL_ENTITY && !m_ecs->isAlive(m_selectedEntity)) {
        m_selectedEntity = ecs::NULL_ENTITY;
    }

    // Clampear scroll
    int maxScroll = std::max(0, (int)m_entityList.size() - MAX_VISIBLE_ENTITIES);
    m_hierarchyScroll = std::min(m_hierarchyScroll, maxScroll);
}

// ════════════════════════════════════════════════════════════════
// Render
// ════════════════════════════════════════════════════════════════

void Editor::render(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                    renderer::SpriteBatch2D& batch) {
    if (!m_active || !m_ecs) return;

    m_ui.begin(m_mouseX, m_mouseY, m_mouseDown, m_mousePressed, batch);

    renderToolbar(shapes, text, batch);
    renderHierarchyPanel(shapes, text, batch);
    renderInspectorPanel(shapes, text, batch);

    m_ui.end();
}

// ── Toolbar ────────────────────────────────────────────────────

void Editor::renderToolbar(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                           renderer::SpriteBatch2D& batch) {
    // Barra superior — coordenadas centradas para ShapeRenderer2D
    auto toShapeCol = [](const math::Color& c) -> renderer::ShapeRenderer2D::Color {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    };
    auto toSpriteCol = [](const math::Color& c) -> renderer::SpriteColor {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    };

    shapes.rectFill(TOOLBAR_X + TOOLBAR_W * 0.5f, TOOLBAR_Y + TOOLBAR_H * 0.5f,
                    TOOLBAR_W, TOOLBAR_H, toShapeCol(math::Color(25, 25, 40, 240)));
    shapes.line(TOOLBAR_X, TOOLBAR_H, TOOLBAR_X + TOOLBAR_W, TOOLBAR_H,
                toShapeCol(math::Color(70, 70, 100)));

    text.draw(batch, "ALZE Editor", 10.0f, 8.0f, 1.0f, toSpriteCol(math::Color(140, 200, 255)));

    // Info de entidades
    char buf[64];
    snprintf(buf, sizeof(buf), "Entities: %d", (int)m_entityList.size());
    text.draw(batch, buf, 200.0f, 8.0f, 1.0f, toSpriteCol(math::Color(160, 170, 190)));

    // Indicador de entidad seleccionada
    if (m_selectedEntity != ecs::NULL_ENTITY) {
        snprintf(buf, sizeof(buf), "Selected: %s", entityLabel(m_selectedEntity).c_str());
        text.draw(batch, buf, 400.0f, 8.0f, 1.0f, toSpriteCol(math::Color(255, 220, 100)));
    }

    // Ayuda
    text.draw(batch, "[F1] Close | Arrows: Move | PgUp/Dn: Y", 600.0f, 8.0f, 1.0f,
              toSpriteCol(math::Color(100, 110, 130)));
}

// ── Panel de jerarquia ─────────────────────────────────────────

void Editor::renderHierarchyPanel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                                  renderer::SpriteBatch2D& batch) {
    m_ui.panel(shapes, text, "Hierarchy", HIERARCHY_X, HIERARCHY_Y, HIERARCHY_W, HIERARCHY_H);

    auto toShapeCol = [](const math::Color& c) -> renderer::ShapeRenderer2D::Color {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    };
    auto toSpriteCol = [](const math::Color& c) -> renderer::SpriteColor {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    };

    float startY = HIERARCHY_Y + 30.0f;
    int visibleCount = std::min((int)m_entityList.size() - m_hierarchyScroll, MAX_VISIBLE_ENTITIES);

    for (int i = 0; i < visibleCount; ++i) {
        int idx = i + m_hierarchyScroll;
        if (idx >= (int)m_entityList.size()) break;

        ecs::Entity entity = m_entityList[idx];
        float y = startY + i * ENTRY_HEIGHT;
        float x = HIERARCHY_X + 5.0f;
        float w = HIERARCHY_W - 10.0f;
        float h = ENTRY_HEIGHT - 2.0f;

        // Highlight seleccionado — coordenadas centradas para rectFill
        bool isSelected = (entity == m_selectedEntity);
        if (isSelected) {
            shapes.rectFill(x + w * 0.5f, y + h * 0.5f, w, h,
                            toShapeCol(math::Color(60, 100, 160, 180)));
        }

        // Boton de seleccion (toda la fila es clickeable)
        std::string lbl = entityLabel(entity);
        if (m_ui.button(shapes, text, lbl, x, y, w, h)) {
            m_selectedEntity = entity;
        }
    }

    // Indicador de scroll si hay mas entidades
    if ((int)m_entityList.size() > MAX_VISIBLE_ENTITIES) {
        char scrollInfo[32];
        snprintf(scrollInfo, sizeof(scrollInfo), "[+/-] %d/%d",
                 m_hierarchyScroll + 1, (int)m_entityList.size());
        text.draw(batch, scrollInfo, HIERARCHY_X + 10.0f, HIERARCHY_Y + HIERARCHY_H - 20.0f,
                  1.0f, toSpriteCol(math::Color(120, 130, 150)));
    }
}

// ── Panel inspector ────────────────────────────────────────────

void Editor::renderInspectorPanel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                                  renderer::SpriteBatch2D& batch) {
    m_ui.panel(shapes, text, "Inspector", INSPECTOR_X, INSPECTOR_Y, INSPECTOR_W, INSPECTOR_H);

    auto toSpriteCol = [](const math::Color& c) -> renderer::SpriteColor {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    };

    if (m_selectedEntity == ecs::NULL_ENTITY || !m_ecs->isAlive(m_selectedEntity)) {
        m_ui.label(text, "No entity selected", INSPECTOR_X + 10.0f, INSPECTOR_Y + 35.0f,
                   math::Color(120, 130, 150));
        return;
    }

    float y = INSPECTOR_Y + 35.0f;
    float x = INSPECTOR_X + 10.0f;
    float w = INSPECTOR_W - 20.0f;
    char buf[128];

    // ── Entity ID ──────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "ID: %u (gen %u)",
             ecs::getIndex(m_selectedEntity), ecs::getGeneration(m_selectedEntity));
    m_ui.label(text, buf, x, y, math::Color(180, 190, 210));
    y += 18.0f;

    m_ui.separator(shapes, x, y, w);
    y += 8.0f;

    // ── Transform3DComponent ───────────────────────────────────
    if (m_ecs->hasComponent<ecs::Transform3DComponent>(m_selectedEntity)) {
        auto& tc = m_ecs->getComponent<ecs::Transform3DComponent>(m_selectedEntity);
        const auto& pos = tc.transform.position;
        const auto& rot = tc.transform.rotation;
        const auto& scl = tc.transform.scale;

        text.draw(batch, "Transform3D", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        snprintf(buf, sizeof(buf), "Pos: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Rot: %.2f, %.2f, %.2f, %.2f", rot.x, rot.y, rot.z, rot.w);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Scl: %.2f, %.2f, %.2f", scl.x, scl.y, scl.z);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 20.0f;

        m_ui.separator(shapes, x, y, w);
        y += 8.0f;
    }

    // ── Physics3DComponent ─────────────────────────────────────
    if (m_ecs->hasComponent<ecs::Physics3DComponent>(m_selectedEntity)) {
        auto& pc = m_ecs->getComponent<ecs::Physics3DComponent>(m_selectedEntity);

        text.draw(batch, "Physics3D", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        snprintf(buf, sizeof(buf), "Vel: %.2f, %.2f, %.2f", pc.velocity.x, pc.velocity.y, pc.velocity.z);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Mass: %.2f  Drag: %.3f", pc.mass, pc.drag);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Rest: %.2f  Fric: %.2f", pc.restitution, pc.friction);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Static: %s  Sleep: %s",
                 pc.isStatic ? "yes" : "no", pc.sleeping ? "yes" : "no");
        m_ui.label(text, buf, x + 5.0f, y);
        y += 20.0f;

        m_ui.separator(shapes, x, y, w);
        y += 8.0f;
    }

    // ── Collider3DComponent ────────────────────────────────────
    if (m_ecs->hasComponent<ecs::Collider3DComponent>(m_selectedEntity)) {
        auto& cc = m_ecs->getComponent<ecs::Collider3DComponent>(m_selectedEntity);

        text.draw(batch, "Collider3D", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        const char* shapeNames[] = { "Sphere", "Box", "Capsule" };
        snprintf(buf, sizeof(buf), "Shape: %s", shapeNames[cc.shape]);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        if (cc.shape == ecs::Collider3DComponent::SPHERE) {
            snprintf(buf, sizeof(buf), "Radius: %.2f", cc.radius);
        } else if (cc.shape == ecs::Collider3DComponent::BOX) {
            snprintf(buf, sizeof(buf), "Half: %.2f, %.2f, %.2f",
                     cc.halfExtents.x, cc.halfExtents.y, cc.halfExtents.z);
        } else {
            snprintf(buf, sizeof(buf), "H: %.2f  R: %.2f", cc.capsuleHeight, cc.radius);
        }
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Layer: %u  Trigger: %s",
                 cc.layer, cc.isTrigger ? "yes" : "no");
        m_ui.label(text, buf, x + 5.0f, y);
        y += 20.0f;

        m_ui.separator(shapes, x, y, w);
        y += 8.0f;
    }

    // ── MeshComponent ──────────────────────────────────────────
    if (m_ecs->hasComponent<ecs::MeshComponent>(m_selectedEntity)) {
        auto& mc = m_ecs->getComponent<ecs::MeshComponent>(m_selectedEntity);

        text.draw(batch, "Mesh", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        snprintf(buf, sizeof(buf), "Visible: %s  Mesh: %s",
                 mc.visible ? "yes" : "no",
                 mc.mesh ? "loaded" : "null");
        m_ui.label(text, buf, x + 5.0f, y);
        y += 20.0f;

        m_ui.separator(shapes, x, y, w);
        y += 8.0f;
    }

    // ── PointLightComponent ────────────────────────────────────
    if (m_ecs->hasComponent<ecs::PointLightComponent>(m_selectedEntity)) {
        auto& lc = m_ecs->getComponent<ecs::PointLightComponent>(m_selectedEntity);

        text.draw(batch, "PointLight", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        snprintf(buf, sizeof(buf), "Color: %.1f, %.1f, %.1f", lc.color.x, lc.color.y, lc.color.z);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 14.0f;

        snprintf(buf, sizeof(buf), "Atten: %.2f / %.3f / %.4f", lc.constant, lc.linear, lc.quadratic);
        m_ui.label(text, buf, x + 5.0f, y);
        y += 20.0f;
    }

    // ── TagComponent ───────────────────────────────────────────
    if (m_ecs->hasComponent<ecs::TagComponent>(m_selectedEntity)) {
        auto& tag = m_ecs->getComponent<ecs::TagComponent>(m_selectedEntity);
        const char* tagNames[] = { "None", "Player", "Enemy", "Projectile", "Ground", "Object" };
        int tagIdx = static_cast<int>(tag.type);
        if (tagIdx < 0 || tagIdx > 5) tagIdx = 0;

        text.draw(batch, "Tag", x, y, 1.0f, toSpriteCol(math::Color(140, 200, 255)));
        y += 16.0f;

        snprintf(buf, sizeof(buf), "Type: %s", tagNames[tagIdx]);
        m_ui.label(text, buf, x + 5.0f, y);
    }
}

// ── Utilidades ─────────────────────────────────────────────────

std::string Editor::entityLabel(ecs::Entity entity) const {
    char buf[48];
    uint32_t idx = ecs::getIndex(entity);

    // Intentar dar un nombre descriptivo basado en componentes
    if (m_ecs->hasComponent<ecs::TagComponent>(entity)) {
        auto& tag = m_ecs->getComponent<ecs::TagComponent>(entity);
        const char* names[] = { "", "Player", "Enemy", "Proj", "Ground", "Obj" };
        int t = static_cast<int>(tag.type);
        if (t > 0 && t <= 5) {
            snprintf(buf, sizeof(buf), "[%d] %s", idx, names[t]);
            return buf;
        }
    }

    // Si tiene luz, indicarlo
    if (m_ecs->hasComponent<ecs::PointLightComponent>(entity)) {
        snprintf(buf, sizeof(buf), "[%d] Light", idx);
        return buf;
    }

    snprintf(buf, sizeof(buf), "[%d] Entity", idx);
    return buf;
}

} // namespace editor
} // namespace engine
