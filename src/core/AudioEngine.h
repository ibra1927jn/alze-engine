#pragma once
// AudioEngine.h — Motor de audio propio (sin SDL_mixer)
// Implementación en AudioEngine.cpp

#include <SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include "Logger.h"
#include "ResourceManager.h"
#include "math/Vector2D.h"
#include "math/Vector3D.h"

namespace engine {
namespace core {

static constexpr int AUDIO_FREQUENCY  = 44100;
static constexpr int AUDIO_CHANNELS   = 2;
static constexpr int AUDIO_SAMPLES    = 2048;
static constexpr int MIXER_VOICES     = 32;
static constexpr SDL_AudioFormat AUDIO_FORMAT = AUDIO_S16SYS;

struct AudioBuffer {
    std::vector<int16_t> samples;
    int sampleRate  = 44100;
    int numChannels = 2;
    std::string name;

    bool isValid()   const { return !samples.empty(); }
    int  numFrames() const { return static_cast<int>(samples.size()) / numChannels; }

    void resampleTo(int targetRate);
    void toStereo();
};

namespace WAVLoader {
    uint16_t readU16LE(const uint8_t* p);
    uint32_t readU32LE(const uint8_t* p);
    bool load(const std::string& path, AudioBuffer& out);
}

struct Voice {
    std::shared_ptr<AudioBuffer> buffer;
    float   position   = 0.0f;
    float   pitchRatio = 1.0f;
    float   volumeL    = 1.0f;
    float   volumeR    = 1.0f;
    int     loops      = 0;
    bool    active     = false;
    int     id         = -1;
    std::string soundId;
};

struct AudioClip {
    std::shared_ptr<AudioBuffer> buffer;
    std::string name;

    AudioClip() = default;
    explicit AudioClip(const std::string& path) : name(path) {
        buffer = std::make_shared<AudioBuffer>();
        if (!WAVLoader::load(path, *buffer)) buffer.reset();
    }
    bool isValid() const { return buffer && buffer->isValid(); }
    AudioClip(const AudioClip&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;
    AudioClip(AudioClip&& o) noexcept : buffer(std::move(o.buffer)), name(std::move(o.name)) {}
};

enum class SoundGroup { MASTER, SFX, MUSIC, UI, VOICE, AMBIENT, COUNT };

class AudioEngine {
public:
    bool init(int frequency = AUDIO_FREQUENCY, int channels = AUDIO_CHANNELS,
              int samples = AUDIO_SAMPLES);
    void shutdown();
    ~AudioEngine() { shutdown(); }

    void setListenerPosition(const math::Vector2D& pos) {
        SDL_LockAudioDevice(m_deviceId);
        m_listenerPos2D = pos;
        SDL_UnlockAudioDevice(m_deviceId);
    }
    math::Vector2D getListenerPosition2D() const {
        SDL_LockAudioDevice(m_deviceId);
        auto pos = m_listenerPos2D;
        SDL_UnlockAudioDevice(m_deviceId);
        return pos;
    }
    void setListener3D(const math::Vector3D& position,
                       const math::Vector3D& forward = math::Vector3D(0, 0, -1),
                       const math::Vector3D& up = math::Vector3D(0, 1, 0));
    math::Vector3D getListenerPosition3D() const {
        SDL_LockAudioDevice(m_deviceId);
        auto pos = m_listenerPos3D;
        SDL_UnlockAudioDevice(m_deviceId);
        return pos;
    }
    void  setMaxDistance(float d) {
        if (d < 0.001f) d = 0.001f;
        SDL_LockAudioDevice(m_deviceId);
        m_maxDist = d;
        SDL_UnlockAudioDevice(m_deviceId);
    }
    float getMaxDistance() const {
        SDL_LockAudioDevice(m_deviceId);
        float d = m_maxDist;
        SDL_UnlockAudioDevice(m_deviceId);
        return d;
    }

    void  setGroupVolume(SoundGroup g, float vol);
    float getGroupVolume(SoundGroup g) const {
        int i = static_cast<int>(g);
        SDL_LockAudioDevice(m_deviceId);
        float vol = (i >= 0 && i < static_cast<int>(SoundGroup::COUNT)) ? m_groupVolumes[i] : 1.0f;
        SDL_UnlockAudioDevice(m_deviceId);
        return vol;
    }
    float getEffectiveVolume(SoundGroup g) const {
        SDL_LockAudioDevice(m_deviceId);
        float vol = m_groupVolumes[static_cast<int>(SoundGroup::MASTER)] *
                    m_groupVolumes[static_cast<int>(g)];
        SDL_UnlockAudioDevice(m_deviceId);
        return vol;
    }

    bool loadSound(const std::string& id, const std::string& path);
    bool loadSoundFromBuffer(const std::string& id, std::shared_ptr<AudioBuffer> buf);

    int  playSound(const std::string& id, SoundGroup group = SoundGroup::SFX, int loops = 0);
    int  playSoundAt(const std::string& id, const math::Vector2D& worldPos,
                     SoundGroup group = SoundGroup::SFX, int loops = 0);
    int  playSoundAt3D(const std::string& id, const math::Vector3D& worldPos,
                       SoundGroup group = SoundGroup::SFX, int loops = 0);

    void stopChannel(int voiceId);
    void stopAllSounds();

    bool loadMusic(const std::string& id, const std::string& path) { return loadSound(id, path); }
    bool playMusic(const std::string& id, int loops = -1);
    bool crossfadeMusic(const std::string& id, int durationMs = 2000);
    void pauseMusic();
    void resumeMusic();
    void stopMusic();
    void setMusicVolume(int vol0to128) { setGroupVolume(SoundGroup::MUSIC, vol0to128 / 128.0f); }
    std::string getCurrentMusic() const {
        SDL_LockAudioDevice(m_deviceId);
        std::string music = m_currentMusic;
        SDL_UnlockAudioDevice(m_deviceId);
        return music;
    }

    void setVoicePitch(int voiceId, float pitch);
    void setMaxInstances(int max) { m_maxInstances = max; }
    int  getMaxInstances() const  { return m_maxInstances; }

    bool isInitialized() const { return m_initialized; }
    int  getSoundsPlayed() const { return m_soundsPlayed.load(); }
    int  getActiveVoices() const {
        // Requiere lock para evitar data race con audioCallback
        SDL_LockAudioDevice(m_deviceId);
        int count = 0;
        for (const auto& v : m_voices) if (v.active) count++;
        SDL_UnlockAudioDevice(m_deviceId);
        return count;
    }

    void update(float dt);

private:
    bool            m_initialized  = false;
    SDL_AudioDeviceID m_deviceId   = 0;
    int             m_sampleRate   = AUDIO_FREQUENCY;
    int             m_outChannels  = AUDIO_CHANNELS;
    math::Vector2D  m_listenerPos2D;
    math::Vector3D  m_listenerPos3D;
    math::Vector3D  m_listenerFwd  {0, 0, -1};
    math::Vector3D  m_listenerUp   {0, 1,  0};
    math::Vector3D  m_listenerRight{1, 0,  0};
    float           m_maxDist      = 50.0f;
    float           m_groupVolumes[static_cast<int>(SoundGroup::COUNT)];
    Voice           m_voices[MIXER_VOICES];
    std::atomic<int> m_soundsPlayed{0};
    int             m_musicVoice   = -1;
    std::string     m_currentMusic;
    float           m_fadeTarget   = 0.0f;
    float           m_fadeDuration = 0.0f;
    float           m_fadeTimer    = 0.0f;
    int             m_maxInstances = 4;
    std::unordered_map<std::string, int> m_instanceCounts;
    ResourceManager<AudioClip> m_sounds;

    int  spawnVoice(std::shared_ptr<AudioBuffer> buf, float volL, float volR,
                    float pitch, int loops, const std::string& soundId);
    bool canPlayInstance(const std::string& id);
    static void audioCallback(void* userdata, Uint8* stream, int len);
};

} // namespace core
} // namespace engine
