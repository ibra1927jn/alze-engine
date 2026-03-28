#pragma once

#include "SpriteBatch2D.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace engine {
namespace renderer {

/// SpriteSheet — Defines a grid of frames within a texture atlas.
///
/// Example: 512x512 texture, 4x4 grid → 16 frames of 128x128 each.
///
struct SpriteSheet {
    const Texture2D* texture = nullptr;
    int columns = 1;     // Frames per row
    int rows    = 1;     // Number of rows
    int frameW  = 0;     // Frame width in pixels (auto-calculated if 0)
    int frameH  = 0;     // Frame height in pixels

    /// Get UV rect for frame at (col, row) — 0-indexed
    SpriteRect getFrame(int col, int row) const {
        if (!texture) return SpriteRect::full();
        int fw = frameW > 0 ? frameW : texture->getWidth() / columns;
        int fh = frameH > 0 ? frameH : texture->getHeight() / rows;
        return SpriteRect::fromPixels(col * fw, row * fh, fw, fh,
                                       texture->getWidth(), texture->getHeight());
    }

    /// Get UV rect for frame by linear index (left-to-right, top-to-bottom)
    SpriteRect getFrame(int index) const {
        int col = index % columns;
        int row = index / columns;
        return getFrame(col, row);
    }

    int totalFrames() const { return columns * rows; }
};

/// AnimationClip — A named sequence of frames with timing.
struct AnimationClip {
    std::string name;
    int startFrame = 0;       // First frame index in the spritesheet
    int frameCount = 1;       // Number of frames in this clip
    float fps = 12.0f;        // Playback speed

    enum class LoopMode {
        ONCE,       // Play once, stop at last frame
        LOOP,       // Loop forever
        PING_PONG   // Forward then backward, repeat
    };
    LoopMode loopMode = LoopMode::LOOP;
};

/// AnimationPlayer — Plays animation clips from a spritesheet.
///
/// Usage:
///   AnimationPlayer player;
///   player.setSheet(&sheet);
///   player.addClip({ "idle", 0, 4, 8.0f, AnimationClip::LoopMode::LOOP });
///   player.addClip({ "run",  4, 6, 12.0f, AnimationClip::LoopMode::LOOP });
///   player.play("idle");
///   // In update loop:
///   player.update(dt);
///   // In render:
///   batch.draw(sheet.texture, pos, size, 0.0f, {0.5f,0.5f}, player.getCurrentFrame());
///
class AnimationPlayer {
public:
    void setSheet(const SpriteSheet* sheet) { m_sheet = sheet; }

    void addClip(const AnimationClip& clip) {
        m_clips[clip.name] = clip;
    }

    /// Start playing a clip by name
    void play(const std::string& name, bool restart = false) {
        if (m_currentClip == name && !restart) return;
        auto it = m_clips.find(name);
        if (it == m_clips.end()) return;
        m_currentClip = name;
        m_time = 0.0f;
        m_finished = false;
        m_pingPongForward = true;
    }

    /// Update animation timer
    void update(float dt) {
        if (m_currentClip.empty() || m_finished) return;
        m_time += dt;
    }

    /// Get the current frame's UV rect
    SpriteRect getCurrentFrame() const {
        if (!m_sheet || m_currentClip.empty()) return SpriteRect::full();

        auto it = m_clips.find(m_currentClip);
        if (it == m_clips.end()) return SpriteRect::full();

        const AnimationClip& clip = it->second;
        float frameDuration = 1.0f / clip.fps;
        int totalFrames = clip.frameCount;
        if (totalFrames <= 0) return SpriteRect::full();

        int frameIndex = 0;

        switch (clip.loopMode) {
            case AnimationClip::LoopMode::LOOP: {
                int rawFrame = static_cast<int>(m_time / frameDuration);
                frameIndex = rawFrame % totalFrames;
                break;
            }
            case AnimationClip::LoopMode::ONCE: {
                int rawFrame = static_cast<int>(m_time / frameDuration);
                frameIndex = rawFrame < totalFrames ? rawFrame : totalFrames - 1;
                if (rawFrame >= totalFrames) m_finished = true;
                break;
            }
            case AnimationClip::LoopMode::PING_PONG: {
                int rawFrame = static_cast<int>(m_time / frameDuration);
                int cycleLen = totalFrames * 2 - 2;
                if (cycleLen <= 0) cycleLen = 1;
                int pos = rawFrame % cycleLen;
                frameIndex = pos < totalFrames ? pos : (cycleLen - pos);
                break;
            }
        }

        return m_sheet->getFrame(clip.startFrame + frameIndex);
    }

    /// Get current clip name
    const std::string& getCurrentClipName() const { return m_currentClip; }
    bool isFinished() const { return m_finished; }
    bool isPlaying() const { return !m_currentClip.empty() && !m_finished; }

    /// Get normalized progress [0..1] of the current clip
    float getProgress() const {
        if (m_currentClip.empty()) return 0.0f;
        auto it = m_clips.find(m_currentClip);
        if (it == m_clips.end()) return 0.0f;
        float totalTime = it->second.frameCount / it->second.fps;
        return totalTime > 0 ? m_time / totalTime : 0.0f;
    }

private:
    const SpriteSheet* m_sheet = nullptr;
    std::unordered_map<std::string, AnimationClip> m_clips;
    std::string m_currentClip;
    float m_time = 0.0f;
    mutable bool m_finished = false;
    bool m_pingPongForward = true;
};

} // namespace renderer
} // namespace engine
