#include "AudioEngine.h"
#include <cstring>
#include <algorithm>

namespace engine {
namespace core {

// ── WAVLoader ────────────────────────────────────────────────────

namespace WAVLoader {

uint16_t readU16LE(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t readU32LE(const uint8_t* p) { return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }

bool load(const std::string& path, AudioBuffer& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) { Logger::warn("WAVLoader", "No se pudo abrir: " + path); return false; }
    Sint64 fileSize = SDL_RWsize(rw);
    if (fileSize <= 0) { SDL_RWclose(rw); return false; }

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    SDL_RWread(rw, data.data(), 1, data.size());
    SDL_RWclose(rw);

    const uint8_t* p = data.data();
    const uint8_t* end = p + data.size();

    if (end - p < 12) { Logger::warn("WAVLoader", "Fichero demasiado pequeño: " + path); return false; }
    if (p[0]!='R'||p[1]!='I'||p[2]!='F'||p[3]!='F') { Logger::warn("WAVLoader", "No es RIFF: " + path); return false; }
    if (p[8]!='W'||p[9]!='A'||p[10]!='V'||p[11]!='E') { Logger::warn("WAVLoader", "No es WAVE: " + path); return false; }
    p += 12;

    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    const uint8_t* pcmData = nullptr;
    size_t pcmSize = 0;

    while (p + 8 <= end) {
        char id[5] = {};
        memcpy(id, p, 4);
        uint32_t chunkSize = readU32LE(p + 4);
        p += 8;
        if (strcmp(id, "fmt ") == 0 && chunkSize >= 16) {
            audioFormat   = readU16LE(p);
            numChannels   = readU16LE(p + 2);
            sampleRate    = readU32LE(p + 4);
            bitsPerSample = readU16LE(p + 14);
        } else if (strcmp(id, "data") == 0) {
            pcmData = p;
            pcmSize = std::min(static_cast<size_t>(chunkSize), static_cast<size_t>(end - p));
        }
        p += (chunkSize + 1) & ~1u;
    }

    if (!pcmData || audioFormat != 1 || numChannels == 0 || sampleRate == 0) {
        Logger::warn("WAVLoader", "Formato no soportado (solo PCM): " + path); return false;
    }

    out.sampleRate = static_cast<int>(sampleRate);
    out.numChannels = numChannels;
    out.name = path;

    if (bitsPerSample == 16) {
        size_t numSamples = pcmSize / 2;
        out.samples.resize(numSamples);
        memcpy(out.samples.data(), pcmData, numSamples * 2);
    } else if (bitsPerSample == 8) {
        out.samples.resize(pcmSize);
        for (size_t i = 0; i < pcmSize; i++)
            out.samples[i] = static_cast<int16_t>((static_cast<int>(pcmData[i]) - 128) * 256);
    } else {
        Logger::warn("WAVLoader", "bitsPerSample=" + std::to_string(bitsPerSample) + " no soportado: " + path);
        return false;
    }

    out.resampleTo(AUDIO_FREQUENCY);
    out.toStereo();
    Logger::info("WAVLoader", "Cargado: " + path + " [" + std::to_string(numChannels) + "ch "
        + std::to_string(sampleRate) + "Hz " + std::to_string(bitsPerSample) + "bps → "
        + std::to_string(out.numFrames()) + " frames]");
    return true;
}

} // namespace WAVLoader

// ── AudioBuffer ──────────────────────────────────────────────────

void AudioBuffer::resampleTo(int targetRate) {
    if (sampleRate <= 0 || sampleRate == targetRate || targetRate <= 0) return;
    float ratio = static_cast<float>(sampleRate) / targetRate;
    int outFrames = static_cast<int>(numFrames() / ratio);
    std::vector<int16_t> out(outFrames * numChannels);
    for (int i = 0; i < outFrames; i++) {
        float srcPos = i * ratio;
        int   srcIdx = static_cast<int>(srcPos);
        float frac   = srcPos - srcIdx;
        int   next   = std::min(srcIdx + 1, numFrames() - 1);
        for (int c = 0; c < numChannels; c++) {
            float a = samples[srcIdx * numChannels + c];
            float b = samples[next   * numChannels + c];
            out[i * numChannels + c] = static_cast<int16_t>(a + frac * (b - a));
        }
    }
    samples = std::move(out);
    sampleRate = targetRate;
}

void AudioBuffer::toStereo() {
    if (numChannels == 2) return;
    std::vector<int16_t> out(samples.size() * 2);
    for (size_t i = 0; i < samples.size(); i++) {
        out[i * 2]     = samples[i];
        out[i * 2 + 1] = samples[i];
    }
    samples = std::move(out);
    numChannels = 2;
}

// ── AudioEngine ──────────────────────────────────────────────────

bool AudioEngine::init(int frequency, int channels, int samples) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        Logger::error("AudioEngine", "SDL_INIT_AUDIO falló: " + std::string(SDL_GetError()));
        return false;
    }
    SDL_AudioSpec desired{}, obtained{};
    desired.freq     = frequency;
    desired.format   = AUDIO_FORMAT;
    desired.channels = static_cast<Uint8>(channels);
    desired.samples  = static_cast<Uint16>(samples);
    desired.callback = audioCallback;
    desired.userdata = this;

    m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (m_deviceId == 0) {
        Logger::error("AudioEngine", "SDL_OpenAudioDevice falló: " + std::string(SDL_GetError()));
        return false;
    }
    m_sampleRate  = obtained.freq;
    m_outChannels = obtained.channels;

    for (int i = 0; i < static_cast<int>(SoundGroup::COUNT); i++) m_groupVolumes[i] = 1.0f;
    for (int i = 0; i < MIXER_VOICES; i++) { m_voices[i].id = i; m_voices[i].active = false; }

    SDL_PauseAudioDevice(m_deviceId, 0);
    m_initialized = true;
    Logger::info("AudioEngine", "Motor de audio propio iniciado: "
        + std::to_string(obtained.freq) + "Hz, "
        + std::to_string(static_cast<int>(obtained.channels)) + " canales, "
        + std::to_string(MIXER_VOICES) + " voces");
    return true;
}

void AudioEngine::shutdown() {
    if (!m_initialized) return;
    SDL_PauseAudioDevice(m_deviceId, 1);
    SDL_CloseAudioDevice(m_deviceId);
    m_deviceId = 0; m_initialized = false;
    Logger::info("AudioEngine", "Cerrado");
}

void AudioEngine::setListener3D(const math::Vector3D& position,
                                 const math::Vector3D& forward,
                                 const math::Vector3D& up)
{
    SDL_LockAudioDevice(m_deviceId);
    m_listenerPos3D = position;
    m_listenerFwd   = forward.normalized();
    m_listenerUp    = up.normalized();
    m_listenerRight = m_listenerFwd.cross(m_listenerUp).normalized();
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::setGroupVolume(SoundGroup g, float vol) {
    int i = static_cast<int>(g);
    if (i >= 0 && i < static_cast<int>(SoundGroup::COUNT)) {
        SDL_LockAudioDevice(m_deviceId);
        m_groupVolumes[i] = std::max(0.0f, std::min(1.0f, vol));
        SDL_UnlockAudioDevice(m_deviceId);
    }
}

bool AudioEngine::loadSound(const std::string& id, const std::string& path) {
    if (!m_initialized) return false;
    auto clip = std::make_shared<AudioClip>(path);
    if (!clip->isValid()) return false;
    m_sounds.insert(id, clip);
    Logger::info("AudioEngine", "Sonido cargado: " + id);
    return true;
}

bool AudioEngine::loadSoundFromBuffer(const std::string& id, std::shared_ptr<AudioBuffer> buf) {
    if (!m_initialized || !buf || !buf->isValid()) return false;
    auto clip = std::make_shared<AudioClip>();
    clip->name = id; clip->buffer = buf;
    m_sounds.insert(id, clip);
    Logger::info("AudioEngine", "Sonido procedural registrado: " + id
        + " [" + std::to_string(buf->numFrames()) + " frames]");
    return true;
}

int AudioEngine::playSound(const std::string& id, SoundGroup group, int loops) {
    if (!m_initialized || !m_sounds.has(id)) return -1;
    if (!canPlayInstance(id)) return -1;
    auto clip = m_sounds.get(id);
    if (!clip || !clip->isValid()) return -1;
    float vol = getEffectiveVolume(group);
    return spawnVoice(clip->buffer, vol, vol, 1.0f, loops, id);
}

int AudioEngine::playSoundAt(const std::string& id, const math::Vector2D& worldPos,
                              SoundGroup group, int loops)
{
    if (!m_initialized || !m_sounds.has(id)) return -1;
    if (!canPlayInstance(id)) return -1;
    auto clip = m_sounds.get(id);
    if (!clip || !clip->isValid()) return -1;

    // Snapshot listener state under lock to avoid data race with setListenerPosition/setMaxDistance
    math::Vector2D listenerPos;
    float maxDist;
    SDL_LockAudioDevice(m_deviceId);
    listenerPos = m_listenerPos2D;
    maxDist = m_maxDist;
    SDL_UnlockAudioDevice(m_deviceId);

    math::Vector2D delta = worldPos - listenerPos;
    float dist = delta.magnitude();
    if (dist > maxDist) return -1;
    float atten = 1.0f - dist / maxDist; atten *= atten;
    float vol = getEffectiveVolume(group) * atten;
    if (vol <= 0.001f) return -1;
    float pan = delta.x / maxDist;
    return spawnVoice(clip->buffer, vol * std::min(1.0f, 1.0f - pan),
                                    vol * std::min(1.0f, 1.0f + pan), 1.0f, loops, id);
}

int AudioEngine::playSoundAt3D(const std::string& id, const math::Vector3D& worldPos,
                                SoundGroup group, int loops)
{
    if (!m_initialized || !m_sounds.has(id)) return -1;
    if (!canPlayInstance(id)) return -1;
    auto clip = m_sounds.get(id);
    if (!clip || !clip->isValid()) return -1;

    // Snapshot listener state under lock to avoid data race with setListener3D/setMaxDistance
    math::Vector3D listenerPos;
    math::Vector3D listenerRight;
    float maxDist;
    SDL_LockAudioDevice(m_deviceId);
    listenerPos = m_listenerPos3D;
    listenerRight = m_listenerRight;
    maxDist = m_maxDist;
    SDL_UnlockAudioDevice(m_deviceId);

    math::Vector3D delta = worldPos - listenerPos;
    float dist = delta.magnitude();
    if (dist > maxDist) return -1;
    float atten = 1.0f - dist / maxDist; atten *= atten;
    float vol = getEffectiveVolume(group) * atten;
    if (vol <= 0.001f) return -1;
    float pan = (dist > 0.001f) ? delta.normalized().dot(listenerRight) : 0.0f;
    return spawnVoice(clip->buffer, vol * std::min(1.0f, 1.0f - pan),
                                    vol * std::min(1.0f, 1.0f + pan), 1.0f, loops, id);
}

void AudioEngine::stopChannel(int voiceId) {
    SDL_LockAudioDevice(m_deviceId);
    if (voiceId >= 0 && voiceId < MIXER_VOICES) m_voices[voiceId].active = false;
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::stopAllSounds() {
    SDL_LockAudioDevice(m_deviceId);
    for (auto& v : m_voices) v.active = false;
    m_instanceCounts.clear();
    SDL_UnlockAudioDevice(m_deviceId);
}

bool AudioEngine::playMusic(const std::string& id, int loops) {
    if (!m_initialized || !m_sounds.has(id)) return false;
    auto clip = m_sounds.get(id);
    if (!clip || !clip->isValid()) return false;
    float vol = getEffectiveVolume(SoundGroup::MUSIC);
    SDL_LockAudioDevice(m_deviceId);
    if (m_musicVoice >= 0 && m_musicVoice < MIXER_VOICES)
        m_voices[m_musicVoice].active = false;
    SDL_UnlockAudioDevice(m_deviceId);
    int voice = spawnVoice(clip->buffer, vol, vol, 1.0f, loops, id);
    SDL_LockAudioDevice(m_deviceId);
    m_musicVoice = voice;
    m_currentMusic = id;
    SDL_UnlockAudioDevice(m_deviceId);
    return true;
}

bool AudioEngine::crossfadeMusic(const std::string& id, int durationMs) {
    if (!m_initialized || !m_sounds.has(id)) return false;
    auto clip = m_sounds.get(id);
    if (!clip || !clip->isValid()) return false;
    float vol = getEffectiveVolume(SoundGroup::MUSIC);
    SDL_LockAudioDevice(m_deviceId);
    if (m_musicVoice >= 0 && m_musicVoice < MIXER_VOICES)
        m_voices[m_musicVoice].active = false;
    SDL_UnlockAudioDevice(m_deviceId);
    int voice = spawnVoice(clip->buffer, 0.0f, 0.0f, 1.0f, -1, id);
    SDL_LockAudioDevice(m_deviceId);
    m_musicVoice = voice;
    m_currentMusic = id;
    m_fadeTarget = vol;
    m_fadeDuration = durationMs / 1000.0f;
    m_fadeTimer = 0.0f;
    SDL_UnlockAudioDevice(m_deviceId);
    return true;
}

void AudioEngine::pauseMusic() {
    SDL_LockAudioDevice(m_deviceId);
    if (m_musicVoice >= 0 && m_musicVoice < MIXER_VOICES)
        m_voices[m_musicVoice].active = false;
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::resumeMusic() {
    SDL_LockAudioDevice(m_deviceId);
    if (m_musicVoice >= 0 && m_musicVoice < MIXER_VOICES)
        m_voices[m_musicVoice].active = true;
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::stopMusic() {
    SDL_LockAudioDevice(m_deviceId);
    if (m_musicVoice >= 0 && m_musicVoice < MIXER_VOICES)
        m_voices[m_musicVoice].active = false;
    m_musicVoice = -1;
    m_currentMusic.clear();
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::setVoicePitch(int voiceId, float pitch) {
    SDL_LockAudioDevice(m_deviceId);
    if (voiceId >= 0 && voiceId < MIXER_VOICES)
        m_voices[voiceId].pitchRatio = std::max(0.1f, pitch);
    SDL_UnlockAudioDevice(m_deviceId);
}

void AudioEngine::update(float dt) {
    if (!m_initialized) return;
    SDL_LockAudioDevice(m_deviceId);
    if (m_fadeDuration > 0.0f && m_musicVoice >= 0) {
        m_fadeTimer += dt;
        float t = std::min(m_fadeTimer / m_fadeDuration, 1.0f);
        if (m_musicVoice < MIXER_VOICES) {
            m_voices[m_musicVoice].volumeL = m_fadeTarget * t;
            m_voices[m_musicVoice].volumeR = m_fadeTarget * t;
        }
        if (t >= 1.0f) m_fadeDuration = 0.0f;
    }
    SDL_UnlockAudioDevice(m_deviceId);
}

int AudioEngine::spawnVoice(std::shared_ptr<AudioBuffer> buf, float volL, float volR,
                             float pitch, int loops, const std::string& soundId)
{
    SDL_LockAudioDevice(m_deviceId);
    int slot = -1;
    for (int i = 0; i < MIXER_VOICES; i++) {
        if (!m_voices[i].active) { slot = i; break; }
    }
    if (slot >= 0) {
        Voice& v = m_voices[slot];
        v.buffer = buf; v.position = 0.0f; v.pitchRatio = pitch;
        v.volumeL = volL; v.volumeR = volR; v.loops = loops;
        v.active = true; v.soundId = soundId;
        m_instanceCounts[soundId]++;
    }
    SDL_UnlockAudioDevice(m_deviceId);
    if (slot >= 0) {
        m_soundsPlayed++;
    }
    return slot;
}

bool AudioEngine::canPlayInstance(const std::string& id) {
    if (m_maxInstances <= 0) return true;
    // Proteger m_instanceCounts — tambien se accede desde audioCallback
    SDL_LockAudioDevice(m_deviceId);
    auto it = m_instanceCounts.find(id);
    bool allowed = (it == m_instanceCounts.end() || it->second < m_maxInstances);
    SDL_UnlockAudioDevice(m_deviceId);
    return allowed;
}

void AudioEngine::audioCallback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<AudioEngine*>(userdata);
    int numFrames = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    memset(out, 0, len);

    for (int vi = 0; vi < MIXER_VOICES; vi++) {
        Voice& v = self->m_voices[vi];
        if (!v.active || !v.buffer) continue;
        const auto& buf = *v.buffer;
        int bufFrames = buf.numFrames();
        if (bufFrames == 0) { v.active = false; continue; }

        for (int i = 0; i < numFrames; i++) {
            int   srcA = static_cast<int>(v.position);
            float frac = v.position - srcA;
            int   srcB = srcA + 1;

            if (srcA >= bufFrames) {
                if (v.loops == 0) { v.active = false; break; }
                if (v.loops > 0) v.loops--;
                v.position = 0.0f; srcA = 0; srcB = 1; frac = 0.0f;
            }
            srcB = std::min(srcB, bufFrames - 1);

            int16_t sL = static_cast<int16_t>(
                buf.samples[srcA * 2]     * (1.0f - frac) + buf.samples[srcB * 2]     * frac);
            int16_t sR = static_cast<int16_t>(
                buf.samples[srcA * 2 + 1] * (1.0f - frac) + buf.samples[srcB * 2 + 1] * frac);

            int32_t mixL = out[i * 2]     + static_cast<int32_t>(sL * v.volumeL);
            int32_t mixR = out[i * 2 + 1] + static_cast<int32_t>(sR * v.volumeR);
            out[i * 2]     = static_cast<int16_t>(std::max(-32768, std::min(32767, mixL)));
            out[i * 2 + 1] = static_cast<int16_t>(std::max(-32768, std::min(32767, mixR)));

            v.position += v.pitchRatio;
        }

        if (!v.active) {
            auto it = self->m_instanceCounts.find(v.soundId);
            if (it != self->m_instanceCounts.end() && it->second > 0) it->second--;
        }
    }
}

} // namespace core
} // namespace engine
