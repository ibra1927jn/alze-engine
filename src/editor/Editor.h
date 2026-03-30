#pragma once

/// Editor de escena minimal — se integra al game loop existente.
///
/// Funcionalidades:
///   - Panel de jerarquia: lista entidades del ECS, seleccionar una
///   - Inspector de propiedades: muestra componentes de la entidad seleccionada
///   - Viewport: mover entidad seleccionada con flechas
///   - Toggle con F1
///
/// Se renderiza sobre la escena usando UISystem (immediate mode).
///

#include "core/UISystem.h"
#include "core/InputManager.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/EntityManager.h"
#include "ecs/Components3D.h"
#include "renderer/ShapeRenderer2D.h"
#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"
#include <string>
#include <vector>

namespace engine {
namespace editor {

class Editor {
public:
    Editor() = default;

    /// Inicializar con referencia al ECS (llamar una vez)
    void init(ecs::ECSCoordinator* ecs);

    /// Procesar input del editor (llamar cada frame antes de update)
    /// Retorna true si el editor consumio el input (el juego no debe procesarlo)
    bool handleInput(const core::InputManager& input, float dt);

    /// Actualizar logica del editor
    void update(float dt);

    /// Renderizar UI del editor sobre la escena
    void render(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                renderer::SpriteBatch2D& batch);

    /// Estado del editor
    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active; }
    void toggle() { m_active = !m_active; }

    /// Entidad seleccionada
    ecs::Entity getSelectedEntity() const { return m_selectedEntity; }

private:
    // Paneles individuales
    void renderHierarchyPanel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                              renderer::SpriteBatch2D& batch);
    void renderInspectorPanel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                              renderer::SpriteBatch2D& batch);
    void renderToolbar(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                       renderer::SpriteBatch2D& batch);

    // Utilidades
    std::string entityLabel(ecs::Entity entity) const;

    // Conversion de colores
    static renderer::ShapeRenderer2D::Color toShapeCol(const math::Color& c) {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    }
    static renderer::SpriteColor toSpriteCol(const math::Color& c) {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    }

    // ── Estado ──────────────────────────────────────────────────
    ecs::ECSCoordinator* m_ecs = nullptr;
    core::UISystem m_ui;

    bool m_active = false;
    ecs::Entity m_selectedEntity = ecs::NULL_ENTITY;

    // Cache de entidades vivas (se refresca cada frame)
    std::vector<ecs::Entity> m_entityList;

    // Scroll del panel de jerarquia
    int m_hierarchyScroll = 0;

    // Velocidad de movimiento del gizmo (unidades/segundo)
    float m_gizmoSpeed = 5.0f;

    // Estado del mouse para el UI
    float m_mouseX = 0, m_mouseY = 0;
    bool m_mouseDown = false;
    bool m_mousePressed = false;
    bool m_prevMouseDown = false; // Para detectar transicion press

    // Layout constantes
    static constexpr float HIERARCHY_X = 10.0f;
    static constexpr float HIERARCHY_Y = 40.0f;
    static constexpr float HIERARCHY_W = 220.0f;
    static constexpr float HIERARCHY_H = 500.0f;

    static constexpr float INSPECTOR_X = 794.0f; // 1024 - 220 - 10
    static constexpr float INSPECTOR_Y = 40.0f;
    static constexpr float INSPECTOR_W = 220.0f;
    static constexpr float INSPECTOR_H = 500.0f;

    static constexpr float TOOLBAR_X = 0.0f;
    static constexpr float TOOLBAR_Y = 0.0f;
    static constexpr float TOOLBAR_W = 1024.0f;
    static constexpr float TOOLBAR_H = 32.0f;

    static constexpr int   MAX_VISIBLE_ENTITIES = 22;
    static constexpr float ENTRY_HEIGHT = 20.0f;
};

} // namespace editor
} // namespace engine
