#pragma once

#include <SDL.h>
#include "Color.h"
#include "AABB.h"
#include "Camera2D.h"
#include <vector>
#include <string>

namespace engine {
namespace core {

/// DebugDraw — Sistema de dibujo de depuración del motor.
///
/// Incluye drawText() con font bitmap integrado de 5x7 píxeles.
/// No necesita SDL_ttf ni fuentes externas.
class DebugDraw {
public:
    static void setEnabled(bool enabled) { s_enabled = enabled; }
    static bool isEnabled() { return s_enabled; }
    static void toggle() { s_enabled = !s_enabled; }

    // ── Primitivas ─────────────────────────────────────────────

    static void drawLine(math::Vector2D a, math::Vector2D b, math::Color color) {
        if (!s_enabled) return;
        s_commands.push_back({Type::LINE, a, b, 0, 0, color, ""});
    }

    static void drawRect(math::Vector2D pos, float w, float h,
                         math::Color color, bool fill = false) {
        if (!s_enabled) return;
        s_commands.push_back({fill ? Type::RECT_FILL : Type::RECT_OUTLINE, pos, {}, w, h, color, ""});
    }

    static void drawAABB(const math::AABB& box, math::Color color) {
        if (!s_enabled) return;
        math::Vector2D center = box.center();
        math::Vector2D size = box.size();
        s_commands.push_back({Type::RECT_OUTLINE, center, {}, size.x, size.y, color, ""});
    }

    static void drawVector(math::Vector2D origin, math::Vector2D vec,
                           math::Color color, float scale = 1.0f) {
        if (!s_enabled) return;
        math::Vector2D end = origin + vec * scale;
        s_commands.push_back({Type::LINE, origin, end, 0, 0, color, ""});
        if (vec.sqrMagnitude() > 1.0f) {
            math::Vector2D dir = vec.normalized();
            math::Vector2D perp = dir.perpendicular();
            math::Vector2D tip1 = end - dir * 6.0f + perp * 3.0f;
            math::Vector2D tip2 = end - dir * 6.0f - perp * 3.0f;
            s_commands.push_back({Type::LINE, end, tip1, 0, 0, color, ""});
            s_commands.push_back({Type::LINE, end, tip2, 0, 0, color, ""});
        }
    }

    static void drawPoint(math::Vector2D pos, math::Color color, float size = 4.0f) {
        if (!s_enabled) return;
        s_commands.push_back({Type::RECT_FILL, pos, {}, size, size, color, ""});
    }

    static void drawCircle(math::Vector2D center, float radius, math::Color color,
                           int segments = 16) {
        if (!s_enabled) return;
        float step = math::MathUtils::TWO_PI / static_cast<float>(segments);
        for (int i = 0; i < segments; i++) {
            float a1 = step * i;
            float a2 = step * (i + 1);
            math::Vector2D p1 = center + math::Vector2D(std::cos(a1), std::sin(a1)) * radius;
            math::Vector2D p2 = center + math::Vector2D(std::cos(a2), std::sin(a2)) * radius;
            s_commands.push_back({Type::LINE, p1, p2, 0, 0, color, ""});
        }
    }

    static void drawCross(math::Vector2D pos, float size, math::Color color) {
        if (!s_enabled) return;
        float h = size * 0.5f;
        drawLine(pos + math::Vector2D(-h, 0), pos + math::Vector2D(h, 0), color);
        drawLine(pos + math::Vector2D(0, -h), pos + math::Vector2D(0, h), color);
    }

    // ── Texto (bitmap font 5x7) ────────────────────────────────

    /// Dibuja texto en una posición (escala = multiplicador de píxeles)
    static void drawText(math::Vector2D pos, const std::string& text,
                         math::Color color, int scale = 1) {
        if (!s_enabled || text.empty()) return;
        s_commands.push_back({Type::TEXT, pos, {}, static_cast<float>(scale), 0, color, text});
    }

    /// Dibuja texto con fondo semitransparente
    static void drawTextBg(math::Vector2D pos, const std::string& text,
                           math::Color color, math::Color bgColor = {0,0,0,180}, int scale = 1) {
        if (!s_enabled || text.empty()) return;
        float w = static_cast<float>(text.length() * 6 * scale + 4);
        float h = static_cast<float>(8 * scale + 4);
        math::Vector2D bgCenter = pos + math::Vector2D(w * 0.5f - 2, h * 0.5f - 2);
        s_commands.push_back({Type::RECT_FILL, bgCenter, {}, w, h, bgColor, ""});
        s_commands.push_back({Type::TEXT, pos, {}, static_cast<float>(scale), 0, color, text});
    }

    // ── Camera ─────────────────────────────────────────────────

    static void setCamera(Camera2D* cam) { s_camera = cam; }

    // ── Render ─────────────────────────────────────────────────

    static void flush(SDL_Renderer* renderer) {
        if (!s_enabled) { s_commands.clear(); return; }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        for (const auto& cmd : s_commands) {
            SDL_SetRenderDrawColor(renderer, cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a);

            // Transform coords through camera if available
            math::Vector2D pa = cmd.a;
            math::Vector2D pb = cmd.b;
            float w = cmd.w;
            float h = cmd.h;
            if (s_camera && cmd.type != Type::TEXT) {
                pa = s_camera->worldToScreen(cmd.a);
                pb = s_camera->worldToScreen(cmd.b);
                w *= s_camera->getZoom();
                h *= s_camera->getZoom();
            }

            switch (cmd.type) {
                case Type::LINE:
                    SDL_RenderDrawLine(renderer,
                        static_cast<int>(pa.x), static_cast<int>(pa.y),
                        static_cast<int>(pb.x), static_cast<int>(pb.y));
                    break;

                case Type::RECT_FILL:
                case Type::RECT_OUTLINE: {
                    SDL_Rect rect = {
                        static_cast<int>(pa.x - w * 0.5f),
                        static_cast<int>(pa.y - h * 0.5f),
                        static_cast<int>(w),
                        static_cast<int>(h)
                    };
                    if (cmd.type == Type::RECT_FILL)
                        SDL_RenderFillRect(renderer, &rect);
                    else
                        SDL_RenderDrawRect(renderer, &rect);
                    break;
                }

                case Type::TEXT:
                    renderText(renderer, pa, cmd.text, cmd.color, static_cast<int>(cmd.w));
                    break;

                default:
                    break;
            }
        }

        s_commands.clear();
    }

    static int commandCount() { return static_cast<int>(s_commands.size()); }

private:
    enum class Type { LINE, RECT_FILL, RECT_OUTLINE, TEXT };

    struct Command {
        Type type;
        math::Vector2D a, b;
        float w = 0.0f, h = 0.0f;
        math::Color color;
        std::string text;
    };

    static inline bool s_enabled = true;
    static inline std::vector<Command> s_commands;
    static inline Camera2D* s_camera = nullptr;

    // ── Bitmap font 5x7 (ASCII 32-126) ────────────────────────

    static void renderText(SDL_Renderer* renderer, math::Vector2D pos,
                           const std::string& text, math::Color color, int scale) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        int cx = static_cast<int>(pos.x);
        int cy = static_cast<int>(pos.y);

        for (char ch : text) {
            if (ch >= 32 && ch <= 126) {
                const uint8_t* glyph = getGlyph(ch);
                for (int row = 0; row < 7; row++) {
                    for (int col = 0; col < 5; col++) {
                        if (glyph[row] & (1 << (4 - col))) {
                            if (scale == 1) {
                                SDL_RenderDrawPoint(renderer, cx + col, cy + row);
                            } else {
                                SDL_Rect pixel = {cx + col * scale, cy + row * scale, scale, scale};
                                SDL_RenderFillRect(renderer, &pixel);
                            }
                        }
                    }
                }
            }
            cx += 6 * scale;
        }
    }

    static const uint8_t* getGlyph(char ch) {
        static const uint8_t font[][7] = {
            {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 space
            {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, // !
            {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, // "
            {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00}, // #
            {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, // $
            {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, // %
            {0x08,0x14,0x14,0x08,0x15,0x12,0x0D}, // &
            {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, // '
            {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, // (
            {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, // )
            {0x04,0x15,0x0E,0x1F,0x0E,0x15,0x04}, // *
            {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // +
            {0x00,0x00,0x00,0x00,0x00,0x04,0x08}, // ,
            {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // -
            {0x00,0x00,0x00,0x00,0x00,0x00,0x04}, // .
            {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, // /
            {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
            {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
            {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
            {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, // 3
            {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
            {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
            {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
            {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
            {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
            {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
            {0x00,0x04,0x00,0x00,0x00,0x04,0x00}, // :
            {0x00,0x04,0x00,0x00,0x00,0x04,0x08}, // ;
            {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, // <
            {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // =
            {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // >
            {0x0E,0x11,0x01,0x06,0x04,0x00,0x04}, // ?
            {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, // @
            {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
            {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
            {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
            {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
            {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
            {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
            {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // G
            {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
            {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
            {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // J
            {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
            {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
            {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
            {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // N
            {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
            {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
            {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
            {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
            {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, // S
            {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
            {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
            {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}, // V
            {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, // W
            {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
            {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // Y
            {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Z
            {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, // [
            {0x10,0x10,0x08,0x04,0x02,0x01,0x01}, // backslash
            {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, // ]
            {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, // ^
            {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, // _
            {0x08,0x04,0x00,0x00,0x00,0x00,0x00}, // `
            {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, // a
            {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E}, // b
            {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E}, // c
            {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F}, // d
            {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, // e
            {0x06,0x08,0x1C,0x08,0x08,0x08,0x08}, // f
            {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E}, // g
            {0x10,0x10,0x16,0x19,0x11,0x11,0x11}, // h
            {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, // i
            {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, // j
            {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, // k
            {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, // l
            {0x00,0x00,0x1A,0x15,0x15,0x11,0x11}, // m
            {0x00,0x00,0x16,0x19,0x11,0x11,0x11}, // n
            {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, // o
            {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, // p
            {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01}, // q
            {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, // r
            {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E}, // s
            {0x08,0x08,0x1C,0x08,0x08,0x09,0x06}, // t
            {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, // u
            {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, // v
            {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, // w
            {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, // x
            {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, // y
            {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, // z
            {0x02,0x04,0x04,0x08,0x04,0x04,0x02}, // {
            {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, // |
            {0x08,0x04,0x04,0x02,0x04,0x04,0x08}, // }
            {0x00,0x00,0x08,0x15,0x02,0x00,0x00}, // ~
        };
        int index = ch - 32;
        if (index < 0 || index > 94) index = 0;
        return font[index];
    }
};

} // namespace core
} // namespace engine
