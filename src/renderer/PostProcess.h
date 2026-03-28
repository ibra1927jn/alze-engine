#pragma once

#include <glad/gl.h>
#include "ShaderProgram.h"
#include <iostream>

namespace engine {
namespace renderer {

// â”€â”€ Post-Processing Shaders â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

namespace PostProcessShaders {

inline const char* QUAD_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

/// Bright-pass with soft knee (smoother transition)
inline const char* BRIGHT_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;   // 0.0 = hard, 0.5 = soft
void main() {
    vec3 color = texture(uScene, vUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = uThreshold * uSoftKnee;
    float soft = brightness - uThreshold + knee;
    soft = clamp(soft / (2.0 * knee + 0.0001), 0.0, 1.0);
    soft = soft * soft;
    float contribution = max(soft, step(uThreshold, brightness));
    FragColor = vec4(color * contribution, 1.0);
}
)GLSL";

/// 13-tap Gaussian â€” wider, smoother bloom
inline const char* BLUR_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uImage;
uniform bool uHorizontal;

void main() {
    vec2 texelSize = 1.0 / textureSize(uImage, 0);
    // 13-tap Gaussian (sigma â‰ˆ 4)
    const float offsets[7] = float[](0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    const float weights[7] = float[](0.1964825501, 0.1747298068, 0.1209853623,
                                      0.0651898684, 0.0273437502, 0.0089195877,
                                      0.0022624445);
    vec3 result = texture(uImage, vUV).rgb * weights[0];

    for (int i = 1; i < 7; i++) {
        vec2 offset = uHorizontal
            ? vec2(texelSize.x * offsets[i], 0.0)
            : vec2(0.0, texelSize.y * offsets[i]);
        result += texture(uImage, vUV + offset).rgb * weights[i];
        result += texture(uImage, vUV - offset).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}
)GLSL";

/// Final composite: ACES Filmic + Bloom + Vignette + FXAA
inline const char* COMPOSITE_FRAG = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uSSAOTex;
uniform float uBloomIntensity;
uniform float uVignetteStrength;
uniform float uExposure;
uniform vec2 uResolution;
uniform bool uFXAAEnabled;
uniform bool uSSAOEnabled;

// â”€â”€ ACES Filmic Tone Mapping â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
vec3 ACESFilmic(vec3 x) {
    // Narkowicz 2015 ACES fit
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// â”€â”€ Chromatic Aberration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
vec3 chromaticAberration(sampler2D tex, vec2 uv, float strength) {
    vec2 dir = uv - 0.5;
    float dist = length(dir);
    vec2 offset = dir * dist * strength;
    float r = texture(tex, uv + offset).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - offset).b;
    return vec3(r, g, b);
}

// â”€â”€ Film Grain (high-quality hash) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
float filmGrain(vec2 uv, float time) {
    float noise = fract(sin(dot(uv * vec2(12.9898, 78.233) + time, vec2(1.0))) * 43758.5453);
    return noise * 2.0 - 1.0;  // [-1, 1]
}

// â”€â”€ Contrast Adaptive Sharpening (AMD CAS simplified) â”€â”€â”€â”€â”€
vec3 contrastSharpen(sampler2D tex, vec2 uv, vec2 invRes, float strength) {
    vec3 c  = texture(tex, uv).rgb;
    vec3 n  = texture(tex, uv + vec2( 0, 1) * invRes).rgb;
    vec3 s  = texture(tex, uv + vec2( 0,-1) * invRes).rgb;
    vec3 e  = texture(tex, uv + vec2( 1, 0) * invRes).rgb;
    vec3 w  = texture(tex, uv + vec2(-1, 0) * invRes).rgb;
    vec3 avg = (n + s + e + w) * 0.25;
    return c + (c - avg) * strength;
}

uniform float uChromaticAberration;  // 0.0 = off, 0.003 = subtle
uniform float uFilmGrain;            // 0.0 = off, 0.03 = subtle
uniform float uSharpenStrength;      // 0.0 = off, 0.5 = moderate
uniform float uTime;                 // For grain animation

// â”€â”€ Color Grading â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
uniform float uColorTemp;            // [-1.0, 1.0] (Cool to Warm)
uniform float uColorTint;            // [-1.0, 1.0] (Green to Magenta)
uniform float uColorContrast;        // 1.0 = normal (0.0 to 2.0+)
uniform float uColorSaturation;      // 1.0 = normal (0.0 = grayscale)

vec3 colorGrade(vec3 color) {
    // 1. Temperature & Tint
    // Approx white balance over linear workspace
    vec3 tempColor = vec3(
        1.0 + uColorTemp * 0.1,    // R
        1.0 + uColorTint * 0.1,    // G
        1.0 - uColorTemp * 0.1     // B
    );
    color *= tempColor;

    // 2. Saturation
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uColorSaturation);

    // 3. Contrast (using standard midpoint 0.5 curve approximation)
    color = (color - 0.5) * uColorContrast + 0.5;

    return max(color, vec3(0.0));
}

// â”€â”€ FXAA (Nvidia quality) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
vec3 applyFXAA(sampler2D tex, vec2 uv, vec2 resolution) {
    vec2 invRes = 1.0 / resolution;

    // Luminance of surrounding pixels
    float lumC  = dot(texture(tex, uv).rgb, vec3(0.299, 0.587, 0.114));
    float lumN  = dot(texture(tex, uv + vec2( 0,  1) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumS  = dot(texture(tex, uv + vec2( 0, -1) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumE  = dot(texture(tex, uv + vec2( 1,  0) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumW  = dot(texture(tex, uv + vec2(-1,  0) * invRes).rgb, vec3(0.299, 0.587, 0.114));

    float lumMin = min(lumC, min(min(lumN, lumS), min(lumE, lumW)));
    float lumMax = max(lumC, max(max(lumN, lumS), max(lumE, lumW)));
    float lumRange = lumMax - lumMin;

    // Skip FXAA if contrast is low
    if (lumRange < max(0.0312, lumMax * 0.125))
        return texture(tex, uv).rgb;

    float lumNW = dot(texture(tex, uv + vec2(-1,  1) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumNE = dot(texture(tex, uv + vec2( 1,  1) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumSW = dot(texture(tex, uv + vec2(-1, -1) * invRes).rgb, vec3(0.299, 0.587, 0.114));
    float lumSE = dot(texture(tex, uv + vec2( 1, -1) * invRes).rgb, vec3(0.299, 0.587, 0.114));

    float edgeH = abs(-2.0 * lumW + lumNW + lumSW) +
                  abs(-2.0 * lumC + lumN  + lumS ) * 2.0 +
                  abs(-2.0 * lumE + lumNE + lumSE);
    float edgeV = abs(-2.0 * lumN + lumNW + lumNE) +
                  abs(-2.0 * lumC + lumW  + lumE ) * 2.0 +
                  abs(-2.0 * lumS + lumSW + lumSE);

    bool isHorizontal = (edgeH >= edgeV);

    float lum1 = isHorizontal ? lumS : lumW;
    float lum2 = isHorizontal ? lumN : lumE;
    float grad1 = abs(lum1 - lumC);
    float grad2 = abs(lum2 - lumC);

    float stepLength = isHorizontal ? invRes.y : invRes.x;
    float gradScaled;
    if (grad1 >= grad2) {
        stepLength = -stepLength;
        gradScaled = grad1;
    } else {
        gradScaled = grad2;
    }

    vec2 currentUV = uv;
    if (isHorizontal) currentUV.y += stepLength * 0.5;
    else              currentUV.x += stepLength * 0.5;

    vec2 offset = isHorizontal ? vec2(invRes.x, 0.0) : vec2(0.0, invRes.y);

    // Walk along edge
    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;

    float lumEnd1 = dot(texture(tex, uv1).rgb, vec3(0.299, 0.587, 0.114)) - lumC;
    float lumEnd2 = dot(texture(tex, uv2).rgb, vec3(0.299, 0.587, 0.114)) - lumC;

    bool done1 = abs(lumEnd1) >= gradScaled;
    bool done2 = abs(lumEnd2) >= gradScaled;

    for (int i = 0; i < 12; i++) {
        if (!done1) { uv1 -= offset; lumEnd1 = dot(texture(tex, uv1).rgb, vec3(0.299, 0.587, 0.114)) - lumC; done1 = abs(lumEnd1) >= gradScaled; }
        if (!done2) { uv2 += offset; lumEnd2 = dot(texture(tex, uv2).rgb, vec3(0.299, 0.587, 0.114)) - lumC; done2 = abs(lumEnd2) >= gradScaled; }
        if (done1 && done2) break;
    }

    float dist1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float dist2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    float pixelOffset = -min(dist1, dist2) / (dist1 + dist2) + 0.5;

    float subPixelFactor = clamp(abs(((lumN + lumS + lumE + lumW) * 0.25 - lumC) /
                                  lumRange), 0.0, 1.0);
    subPixelFactor = smoothstep(0.0, 1.0, subPixelFactor) * 0.75;

    float finalOffset = max(pixelOffset, subPixelFactor);
    vec2 finalUV = uv;
    if (isHorizontal) finalUV.y += finalOffset * stepLength;
    else              finalUV.x += finalOffset * stepLength;

    return texture(tex, finalUV).rgb;
}

void main() {
    // Scene color (with optional FXAA)
    vec3 scene;
    if (uFXAAEnabled)
        scene = applyFXAA(uScene, vUV, uResolution);
    else
        scene = texture(uScene, vUV).rgb;

    // SSAO â€” multiply occlusion into scene before bloom
    if (uSSAOEnabled) {
        float ao = texture(uSSAOTex, vUV).r;
        scene *= ao;
    }

    // Sharpen (before bloom add)
    if (uSharpenStrength > 0.0) {
        vec2 invRes = 1.0 / uResolution;
        scene = contrastSharpen(uScene, vUV, invRes, uSharpenStrength);
        if (uSSAOEnabled) scene *= texture(uSSAOTex, vUV).r;
    }

    // Bloom
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 color = scene + bloom * uBloomIntensity;

    // Exposure
    color *= uExposure;

    // Color Grading (before Tonemap)
    color = colorGrade(color);

    // ACES Filmic tone mapping
    color = ACESFilmic(color);

    // Chromatic aberration (subtle, edges only)
    if (uChromaticAberration > 0.0) {
        vec3 ca = chromaticAberration(uScene, vUV, uChromaticAberration);
        // Blend CA into tonemapped result weighted by distance from center
        float dist = length(vUV - 0.5);
        color = mix(color, ACESFilmic(ca * uExposure + bloom * uBloomIntensity), dist * 0.5);
    }

    // Vignette (smooth)
    vec2 uv = vUV * 2.0 - 1.0;
    float vignette = 1.0 - dot(uv * 0.7, uv * 0.7) * uVignetteStrength;
    vignette = smoothstep(0.0, 1.0, vignette);
    color *= vignette;

    // Film grain (cinematic subtle noise)
    if (uFilmGrain > 0.0) {
        float grain = filmGrain(vUV * uResolution, uTime);
        float luminance = dot(color, vec3(0.299, 0.587, 0.114));
        // Less grain in bright areas, more in dark (photographic behavior)
        float grainAmount = uFilmGrain * (1.0 - luminance * 0.5);
        color += grain * grainAmount;
    }

    // Gamma correction
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
)GLSL";

} // namespace PostProcessShaders

/// PostProcess settings
struct PostProcessSettings {
    float bloomThreshold   = 0.8f;
    float bloomIntensity   = 0.25f;
    float bloomSoftKnee    = 0.5f;
    int   bloomPasses      = 6;
    float vignetteStrength = 0.25f;
    float exposure         = 1.5f;
    bool  fxaaEnabled      = true;
    bool  ssaoEnabled      = false;
    GLuint ssaoTexture     = 0;
    float chromaticAberration = 0.0f;   // 0 = off, 0.003 = subtle
    float filmGrain        = 0.0f;      // 0 = off, 0.02 = cinematic
    float sharpenStrength  = 0.0f;      // 0 = off, 0.3 = moderate
    float time             = 0.0f;
    bool  halfResBloom     = true;
    
    // Color Grading (neutral defaults)
    float colorTemp        = 0.0f;
    float colorTint        = 0.0f;
    float colorContrast    = 1.0f;      // 1.0 = neutral
    float colorSaturation  = 1.0f;      // 1.0 = neutral
};

/// PostProcess v2 â€” HDR + Bloom + FXAA + Vignette + ACES Filmic.
class PostProcess {
public:
    using Settings = PostProcessSettings;

    PostProcess() = default;
    ~PostProcess() { destroy(); }

    bool init(int width, int height);
    void resize(int width, int height);
    void beginScene();
    GLuint getDepthTexture() const { return m_hdrDepthTex; }
    GLuint getHDRColorTexture() const { return m_hdrColorTex; }
    void endScene(const Settings& settings = Settings());

private:
    void bindTex(GLuint tex, int slot);
    bool createFBOs(int w, int h);
    void createQuad();
    void drawQuad();
    void destroyFBOs();
    void destroy();

    ShaderProgram m_brightShader, m_blurShader, m_compositeShader;
    GLuint m_hdrFBO = 0, m_hdrColorTex = 0, m_hdrDepthTex = 0;
    GLuint m_pingpongFBO[2] = {0, 0}, m_pingpongTex[2] = {0, 0};
    GLuint m_quadVAO = 0, m_quadVBO = 0, m_quadEBO = 0;
    int m_width = 0, m_height = 0;
};

} // namespace renderer
} // namespace engine
