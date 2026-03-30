#pragma once

#include "SpriteBatch2D.h"
#include "Texture2D.h"
#include "math/Vector2D.h"
#include <string>

namespace engine {
namespace renderer {

struct BitmapFont {
    const Texture2D* texture = nullptr;
    int glyphW = 8, glyphH = 12;
    int atlasColumns = 16, atlasRows = 8;
    int firstChar = 32;
    float spacing = 1.0f;

    SpriteRect getGlyph(char c) const {
        int idx = static_cast<int>(c) - firstChar;
        if (idx < 0 || idx >= atlasColumns * atlasRows) idx = 0;
        int col = idx % atlasColumns, row = idx / atlasColumns;
        if (!texture) return SpriteRect::full();
        return SpriteRect::fromPixels(col * glyphW, row * glyphH, glyphW, glyphH,
                                      texture->getWidth(), texture->getHeight());
    }
};

enum class TextAlign { LEFT, CENTER, RIGHT };

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer() = default;

    bool init();
    void setFont(const BitmapFont& font) { m_font = font; }
    const BitmapFont& getFont() const { return m_font; }

    void draw(SpriteBatch2D& batch, const std::string& text,
              float x, float y, float scale = 1.0f,
              const SpriteColor& color = SpriteColor::white(),
              float depth = 100.0f) const;

    void drawAligned(SpriteBatch2D& batch, const std::string& text,
                     float x, float y, TextAlign align = TextAlign::LEFT,
                     float scale = 1.0f,
                     const SpriteColor& color = SpriteColor::white(),
                     float depth = 100.0f) const;

    void drawWrapped(SpriteBatch2D& batch, const std::string& text,
                     float x, float y, float maxWidth,
                     float scale = 1.0f,
                     const SpriteColor& color = SpriteColor::white(),
                     float depth = 100.0f) const;

    float measureWidth(const std::string& text, float scale = 1.0f) const;
    float measureHeight(const std::string& text, float scale = 1.0f) const;

private:
    BitmapFont m_font;
    Texture2D m_builtinTexture;
    void generateBuiltinFont();
};

} // namespace renderer
} // namespace engine
