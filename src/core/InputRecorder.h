#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <cstdint>
#include "math/Vector2D.h"

namespace engine {
namespace core {

/// InputRecorder — Graba y reproduce secuencias de input.
///
/// Graba todos los eventos de teclado, ratón y scroll por frame.
/// Permite reproducir la sesión exacta para debugging determinista.
///
/// Uso:
///   InputRecorder::startRecording("session.inp");
///   // Cada frame en handleInput:
///   InputRecorder::recordFrame(frameNum, keys, mousePos, scroll, mouseBtn);
///   InputRecorder::stopRecording();
///
///   InputRecorder::startPlayback("session.inp");
///   // Cada frame:
///   auto frame = InputRecorder::getPlaybackFrame(frameNum);
///
class InputRecorder {
public:
    /// Estado de un frame grabado
    struct InputFrame {
        int frameNumber = 0;
        uint32_t keysDown[8] = {};          // Bitmap: 256 keys (8 * 32 bits)
        float mouseX = 0, mouseY = 0;
        int scrollDelta = 0;
        uint32_t mouseButtons = 0;          // Bitmap de botones del ratón

        void setKey(int scancode, bool down) {
            int idx = scancode / 32;
            int bit = scancode % 32;
            if (idx < 8) {
                if (down) keysDown[idx] |= (1u << bit);
                else      keysDown[idx] &= ~(1u << bit);
            }
        }

        bool getKey(int scancode) const {
            int idx = scancode / 32;
            int bit = scancode % 32;
            return (idx < 8) && (keysDown[idx] & (1u << bit));
        }
    };

    // ══════════════════════════════════════════════════════════
    // Recording
    // ══════════════════════════════════════════════════════════

    static void startRecording(const std::string& path) {
        stopRecording();
        s_recFile = std::fopen(path.c_str(), "wb");
        s_recording = (s_recFile != nullptr);
        s_recFrameCount = 0;
        s_recPath = path;

        if (s_recording) {
            // Write header
            const char magic[] = "INPR";
            std::fwrite(magic, 1, 4, s_recFile);
            int version = 1;
            std::fwrite(&version, sizeof(int), 1, s_recFile);
        }
    }

    static void recordFrame(const InputFrame& frame) {
        if (!s_recording) return;
        std::fwrite(&frame, sizeof(InputFrame), 1, s_recFile);
        s_recFrameCount++;
        if (s_recFrameCount % 60 == 0) std::fflush(s_recFile);
    }

    static void stopRecording() {
        if (s_recFile) {
            std::fflush(s_recFile);
            std::fclose(s_recFile);
            s_recFile = nullptr;
        }
        s_recording = false;
    }

    // ══════════════════════════════════════════════════════════
    // Playback
    // ══════════════════════════════════════════════════════════

    static bool startPlayback(const std::string& path) {
        stopPlayback();
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) return false;

        // Read header
        char magic[4];
        if (std::fread(magic, 1, 4, file) != 4 ||
            magic[0] != 'I' || magic[1] != 'N' || magic[2] != 'P' || magic[3] != 'R') {
            std::fclose(file);
            return false;
        }

        int version;
        if (std::fread(&version, sizeof(int), 1, file) != 1 || version != 1) {
            std::fclose(file);
            return false;
        }

        // Read all frames
        s_playbackFrames.clear();
        InputFrame frame;
        while (std::fread(&frame, sizeof(InputFrame), 1, file) == 1) {
            s_playbackFrames.push_back(frame);
        }
        std::fclose(file);

        s_playbackIndex = 0;
        s_playing = !s_playbackFrames.empty();
        s_playPath = path;
        return s_playing;
    }

    /// Obtener frame de playback actual (avanza automáticamente)
    static const InputFrame* getNextFrame() {
        if (!s_playing || s_playbackIndex >= static_cast<int>(s_playbackFrames.size())) {
            s_playing = false;
            return nullptr;
        }
        return &s_playbackFrames[s_playbackIndex++];
    }

    static void stopPlayback() {
        s_playbackFrames.clear();
        s_playing = false;
        s_playbackIndex = 0;
    }

    // ══════════════════════════════════════════════════════════
    // Status
    // ══════════════════════════════════════════════════════════

    static bool isRecording() { return s_recording; }
    static bool isPlaying() { return s_playing; }
    static int getRecordedFrames() { return s_recFrameCount; }
    static int getPlaybackTotal() { return static_cast<int>(s_playbackFrames.size()); }
    static int getPlaybackIndex() { return s_playbackIndex; }
    static const std::string& getRecordPath() { return s_recPath; }
    static const std::string& getPlaybackPath() { return s_playPath; }

private:
    // Recording
    static inline std::FILE* s_recFile = nullptr;
    static inline bool s_recording = false;
    static inline int s_recFrameCount = 0;
    static inline std::string s_recPath;

    // Playback
    static inline std::vector<InputFrame> s_playbackFrames;
    static inline bool s_playing = false;
    static inline int s_playbackIndex = 0;
    static inline std::string s_playPath;
};

} // namespace core
} // namespace engine
