#pragma once

// ShaderLibrary v5 — Carga shaders desde archivos .glsl con fallback a embebidos.
// Los shaders se buscan en assets/shaders/ al llamar a init().
// Si no se encuentran, se usan los string literals embebidos como respaldo.

#include "ShaderProgram.h"
#include "ShaderLoader.h"
#include <string>

namespace engine {
namespace renderer {

namespace ShaderLibrary {

// ── Estado interno: almacena shaders cargados desde archivo ─────
// Necesitamos strings persistentes para que los punteros c_str() sigan validos.
namespace detail {
    inline std::string s_flat2dVert, s_flat2dFrag;
    inline std::string s_depthVert, s_depthFrag;
    inline std::string s_pbrVert, s_pbrFrag;
    inline std::string s_unlit3dVert, s_unlit3dFrag;
    inline bool s_initialized = false;
} // namespace detail

// ════════════════════════════════════════════════════════════════
// FALLBACK — Shaders embebidos (string literals) para cuando
// los archivos .glsl no estan disponibles
// ════════════════════════════════════════════════════════════════

namespace fallback {

inline const char* FLAT2D_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
uniform mat4 uProjection;
out vec4 vColor;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

inline const char* FLAT2D_FRAG = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)GLSL";

inline const char* DEPTH_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
)GLSL";

inline const char* DEPTH_FRAG = R"GLSL(
#version 330 core
void main() {}
)GLSL";

inline const char* PBR_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform mat4 uLightSpaceMatrix[2];

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vFragPosLightSpace[2];
out float vViewDepth;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    vFragPosLightSpace[0] = uLightSpaceMatrix[0] * worldPos;
    vFragPosLightSpace[1] = uLightSpaceMatrix[1] * worldPos;
    vec4 viewPos = uView * worldPos;
    vViewDepth = -viewPos.z;
    gl_Position = uProjection * viewPos;
}
)GLSL";

inline const char* PBR_FRAG = R"GLSL(
#version 330 core
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
#define PI 3.14159265359
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vFragPosLightSpace[2];
in float vViewDepth;
out vec4 FragColor;
uniform vec3 uAlbedo; uniform float uMetallic; uniform float uRoughness; uniform float uAo;
uniform sampler2D uAlbedoTex; uniform bool uUseAlbedoTex;
uniform sampler2D uNormalMap; uniform bool uUseNormalMap;
uniform sampler2D uMetallicRoughnessTex; uniform bool uUseMetallicRoughnessTex;
uniform vec3 uEmissiveColor; uniform float uEmissiveIntensity;
uniform sampler2D uEmissiveTex; uniform bool uUseEmissiveTex;
uniform sampler2D uAoTex; uniform bool uUseAoTex;
uniform sampler2D uHeightMap; uniform bool uUseHeightMap; uniform float uParallaxScale;
uniform vec3 uDirLightDir; uniform vec3 uDirLightColor;
uniform samplerCube uIrradianceMap; uniform samplerCube uPrefilterMap;
uniform sampler2D uBRDFLUT; uniform bool uIBLEnabled; uniform float uIBLIntensity;
uniform vec3 uSkyColor; uniform vec3 uGroundColor; uniform float uAmbientIntensity;
struct PointLight { vec3 position; vec3 color; float constant; float linear; float quadratic; };
uniform PointLight uPointLights[MAX_POINT_LIGHTS]; uniform int uNumPointLights;
struct SpotLight { vec3 position; vec3 direction; vec3 color; float cutOff; float outerCutOff; float constant; float linear; float quadratic; };
uniform SpotLight uSpotLights[MAX_SPOT_LIGHTS]; uniform int uNumSpotLights;
uniform sampler2DShadow uShadowMap[2]; uniform bool uShadowEnabled;
uniform float uCascadeSplit; uniform vec3 uViewPos;
uniform mat4 uLightSpaceMatrix[2]; uniform int uRenderMode;
float filterRoughness(vec3 N, float roughness) { vec3 dndu = dFdx(N); vec3 dndv = dFdy(N); float variance = dot(dndu, dndu) + dot(dndv, dndv); float kernelRoughness = min(variance * 0.5, 0.18); return clamp(sqrt(roughness * roughness + kernelRoughness), 0.0, 1.0); }
float DistributionGGX(vec3 N, vec3 H, float roughness) { float a = roughness * roughness; float a2 = a * a; float NdotH = max(dot(N, H), 0.0); float NdotH2 = NdotH * NdotH; float denom = (NdotH2 * (a2 - 1.0) + 1.0); denom = PI * denom * denom; return a2 / max(denom, 0.0001); }
float GeometrySmithGGX(float NdotV, float NdotL, float roughness) { float a2 = roughness * roughness * roughness * roughness; float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2); float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2); return 0.5 / max(ggxV + ggxL, 0.0001); }
vec3 fresnelSchlick(float cosTheta, vec3 F0) { float f = 1.0 - cosTheta; return F0 + (1.0 - F0) * (f * f * f * f * f); }
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) { float f = 1.0 - cosTheta; float f5 = f * f * f * f * f; return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f5; }
vec3 multiscatterCompensation(vec3 F0, vec3 F, float NdotV, float roughness) { float Eavg = 1.0 - (roughness * 0.5); vec3 FssEss = F; vec3 Fms = FssEss * FssEss * Eavg / (1.0 - FssEss * (1.0 - Eavg)); return vec3(1.0) + Fms / max(FssEss, vec3(0.001)); }
vec2 vogelDiskSample(int sampleIndex, int samplesCount, float phi) { float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount)); float theta = float(sampleIndex) * 2.39996323 + phi; return vec2(r * cos(theta), r * sin(theta)); }
float interleavedGradientNoise(vec2 position_screen) { vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189); return fract(magic.z * fract(dot(position_screen, magic.xy))); }
float calcShadowCascade(sampler2DShadow shadowMap, vec4 fragPosLightSpace, vec3 N, vec3 lightDir) { vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; projCoords = projCoords * 0.5 + 0.5; if (projCoords.z > 1.0) return 0.0; float cosAngle = max(dot(N, lightDir), 0.0); float slopeScale = sqrt(1.0 - cosAngle * cosAngle) / max(cosAngle, 0.001); float bias = max(0.0005 * slopeScale, 0.0002); float currentDepth = projCoords.z - bias; vec2 fade = smoothstep(vec2(0.0), vec2(0.05), projCoords.xy) * (1.0 - smoothstep(vec2(0.95), vec2(1.0), projCoords.xy)); float edgeFade = fade.x * fade.y; float radiusScale = mix(2.0, 4.0, vViewDepth / 50.0); vec2 texelSize = 1.0 / textureSize(shadowMap, 0); float shadow = 0.0; const int NUM_SAMPLES = 16; float noise = interleavedGradientNoise(gl_FragCoord.xy); float spinAngle = noise * 2.0 * PI; for (int i = 0; i < NUM_SAMPLES; i++) { vec2 offset = vogelDiskSample(i, NUM_SAMPLES, spinAngle); vec3 sc = vec3(projCoords.xy + offset * texelSize * radiusScale, currentDepth); shadow += 1.0 - texture(shadowMap, sc); } return (shadow / float(NUM_SAMPLES)) * edgeFade; }
uniform float uVolumetricIntensity; uniform float uFogDensity;
float calcVolumetric(vec3 rayOrigin, vec3 rayDir, float rayLength, vec3 L) { const int NUM_STEPS = 16; float stepSize = rayLength / float(NUM_STEPS); vec3 stepVec = rayDir * stepSize; float noise = interleavedGradientNoise(gl_FragCoord.xy); vec3 currentPos = rayOrigin + stepVec * noise; float vLight = 0.0; for(int i = 0; i < NUM_STEPS; i++) { vec4 fragPosLS = uLightSpaceMatrix[0] * vec4(currentPos, 1.0); vec3 projCoords = fragPosLS.xyz / fragPosLS.w; projCoords = projCoords * 0.5 + 0.5; if(projCoords.z <= 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0) { float shadow = texture(uShadowMap[0], vec3(projCoords.xy, projCoords.z - 0.001)); vLight += shadow; } currentPos += stepVec; } float g = 0.6; float cosTheta = dot(rayDir, L); float phase = (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0*g*cosTheta, 1.5)); return (vLight / float(NUM_STEPS)) * phase * rayLength * 0.02; }
float calcShadowCSM(vec3 N, vec3 lightDir) { float blendZone = uCascadeSplit * 0.1; if (vViewDepth < uCascadeSplit - blendZone) { return calcShadowCascade(uShadowMap[0], vFragPosLightSpace[0], N, lightDir); } else if (vViewDepth < uCascadeSplit + blendZone) { float t = (vViewDepth - (uCascadeSplit - blendZone)) / (2.0 * blendZone); float s0 = calcShadowCascade(uShadowMap[0], vFragPosLightSpace[0], N, lightDir); float s1 = calcShadowCascade(uShadowMap[1], vFragPosLightSpace[1], N, lightDir); return mix(s0, s1, t); } else { return calcShadowCascade(uShadowMap[1], vFragPosLightSpace[1], N, lightDir); } }
vec3 getNormal() { vec3 N = normalize(vNormal); if (!uUseNormalMap) return N; vec3 pos_dx = dFdx(vFragPos); vec3 pos_dy = dFdy(vFragPos); vec2 tex_dx = dFdx(vTexCoord); vec2 tex_dy = dFdy(vTexCoord); vec3 T = normalize(pos_dx * tex_dy.y - pos_dy * tex_dx.y); vec3 B = normalize(cross(N, T)); mat3 TBN = mat3(T, B, N); vec3 tangentNormal = texture(uNormalMap, vTexCoord).xyz * 2.0 - 1.0; return normalize(TBN * tangentNormal); }
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) { float numSteps = mix(8.0, 32.0, abs(dot(vec3(0,0,1), viewDir))); float layerDepth = 1.0 / numSteps; float currentLayerDepth = 0.0; vec2 P = viewDir.xy / viewDir.z * uParallaxScale; vec2 deltaTexCoords = P / numSteps; vec2 currentTexCoords = texCoords; float currentDepthMapValue = texture(uHeightMap, currentTexCoords).r; while (currentLayerDepth < currentDepthMapValue) { currentTexCoords -= deltaTexCoords; currentDepthMapValue = texture(uHeightMap, currentTexCoords).r; currentLayerDepth += layerDepth; } vec2 prevTexCoords = currentTexCoords + deltaTexCoords; float afterDepth = currentDepthMapValue - currentLayerDepth; float beforeDepth = texture(uHeightMap, prevTexCoords).r - currentLayerDepth + layerDepth; float weight = afterDepth / (afterDepth - beforeDepth); return mix(currentTexCoords, prevTexCoords, weight); }
vec3 calcPBRLight(vec3 L, vec3 lightColor, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) { vec3 H = normalize(V + L); float NdotV = max(dot(N, V), 0.0); float NdotL = max(dot(N, L), 0.0); float HdotV = max(dot(H, V), 0.0); float NDF = DistributionGGX(N, H, roughness); float G = GeometrySmithGGX(NdotV, NdotL, roughness); vec3 F = fresnelSchlick(HdotV, F0); vec3 specular = NDF * G * F; vec3 kS = F; vec3 kD = (1.0 - kS) * (1.0 - metallic); vec3 compensation = multiscatterCompensation(F0, F, NdotV, roughness); specular *= compensation; return (kD * albedo / PI + specular) * lightColor * NdotL; }
void main() {
    vec2 texCoord = vTexCoord;
    if (uUseHeightMap) { vec3 pos_dx = dFdx(vFragPos); vec3 pos_dy = dFdy(vFragPos); vec2 tex_dx = dFdx(vTexCoord); vec2 tex_dy = dFdy(vTexCoord); vec3 T = normalize(pos_dx * tex_dy.y - pos_dy * tex_dx.y); vec3 B = normalize(cross(normalize(vNormal), T)); vec3 N = normalize(vNormal); mat3 TBN = mat3(T, B, N); vec3 viewDirTangent = normalize(transpose(TBN) * (uViewPos - vFragPos)); texCoord = parallaxMapping(vTexCoord, viewDirTangent); if (texCoord.x > 1.0 || texCoord.y > 1.0 || texCoord.x < 0.0 || texCoord.y < 0.0) discard; }
    vec3 albedo = uAlbedo; if (uUseAlbedoTex) albedo *= texture(uAlbedoTex, texCoord).rgb;
    float metallic = uMetallic; float roughness = uRoughness;
    if (uUseMetallicRoughnessTex) { vec4 mr = texture(uMetallicRoughnessTex, texCoord); roughness *= mr.g; metallic *= mr.b; }
    roughness = max(roughness, 0.04);
    float ao = uAo; if (uUseAoTex) ao *= texture(uAoTex, texCoord).r;
    vec3 N = getNormal(); vec3 V = normalize(uViewPos - vFragPos);
    roughness = filterRoughness(N, roughness);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float shadow = 0.0; vec3 lightDir = normalize(uDirLightDir);
    if (uShadowEnabled) shadow = calcShadowCSM(N, lightDir);
    vec3 Lo = calcPBRLight(lightDir, uDirLightColor, N, V, albedo, metallic, roughness, F0);
    Lo *= (1.0 - shadow * 0.85);
    if (uShadowEnabled && uVolumetricIntensity > 0.0) { float rayLength = length(vFragPos - uViewPos); vec3 rayDir = (vFragPos - uViewPos) / rayLength; float vLight = calcVolumetric(uViewPos, rayDir, min(rayLength, uCascadeSplit), lightDir); Lo += uDirLightColor * vLight * uVolumetricIntensity; }
    for (int i = 0; i < uNumPointLights; i++) { vec3 lightVec = uPointLights[i].position - vFragPos; float dist = length(lightVec); vec3 L = lightVec / dist; float attenuation = 1.0 / (uPointLights[i].constant + uPointLights[i].linear * dist + uPointLights[i].quadratic * dist * dist); vec3 radiance = uPointLights[i].color * attenuation; Lo += calcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0); }
    for (int i = 0; i < uNumSpotLights; i++) { vec3 lightVec = uSpotLights[i].position - vFragPos; float dist = length(lightVec); vec3 L = lightVec / dist; float theta = dot(L, normalize(-uSpotLights[i].direction)); float epsilon = uSpotLights[i].cutOff - uSpotLights[i].outerCutOff; float intensity = clamp((theta - uSpotLights[i].outerCutOff) / epsilon, 0.0, 1.0); float attenuation = 1.0 / (uSpotLights[i].constant + uSpotLights[i].linear * dist + uSpotLights[i].quadratic * dist * dist); vec3 radiance = uSpotLights[i].color * attenuation * intensity; Lo += calcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0); }
    float NdotV = max(dot(N, V), 0.0); vec3 kS = fresnelSchlickRoughness(NdotV, F0, roughness); vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 ambient;
    if (uIBLEnabled) { vec3 irradiance = texture(uIrradianceMap, N).rgb; vec3 diffuseIBL = kD * albedo * irradiance; vec3 R = reflect(-V, N); const float MAX_REFLECTION_LOD = 4.0; vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb; vec2 brdf = texture(uBRDFLUT, vec2(NdotV, roughness)).rg; vec3 specularIBL = prefilteredColor * (kS * brdf.x + brdf.y); ambient = (diffuseIBL + specularIBL) * ao * uIBLIntensity; }
    else { float hemisphere = N.y * 0.5 + 0.5; vec3 ambientColor = mix(uGroundColor, uSkyColor, hemisphere); vec3 R = reflect(-V, N); float envHemi = R.y * 0.5 + 0.5; vec3 envColor = mix(uGroundColor * 2.0, uSkyColor * 4.0, envHemi); vec3 diffuseAmbient = kD * albedo * ambientColor; float smoothness = 1.0 - roughness * roughness; vec3 specEnv = mix(ambientColor, envColor, smoothness); vec3 specularAmbient = kS * specEnv; ambient = (diffuseAmbient + specularAmbient) * uAmbientIntensity * ao; }
    vec3 emissive = uEmissiveColor * uEmissiveIntensity; if (uUseEmissiveTex) emissive *= texture(uEmissiveTex, vTexCoord).rgb;
    vec3 color = ambient + Lo + emissive;
    if (uRenderMode == 1) { float NdotL_toon = max(dot(N, lightDir), 0.0); float toonLight; if (NdotL_toon > 0.8) toonLight = 1.0; else if (NdotL_toon > 0.4) toonLight = 0.7; else if (NdotL_toon > 0.15) toonLight = 0.4; else toonLight = 0.2; float edgeFactor = dot(N, V); float edge = smoothstep(0.0, 0.25, edgeFactor); float lum = dot(albedo, vec3(0.299, 0.587, 0.114)); vec3 saturated = mix(vec3(lum), albedo, 1.5); vec3 toonColor = saturated * toonLight * uDirLightColor; toonColor += emissive; toonColor *= edge; color = toonColor; }
    else if (uRenderMode == 2) { float fresnel = 1.0 - max(dot(N, V), 0.0); fresnel = pow(fresnel, 2.0); vec3 neonColor = mix(albedo * 2.0, vec3(0.4, 0.8, 1.0), 0.3); vec3 glow = neonColor * fresnel * 3.0; float NdotL_neon = max(dot(N, lightDir), 0.0); vec3 interior = albedo * 0.05 * (0.5 + NdotL_neon * 0.5); float gridX = abs(fract(vTexCoord.x * 8.0) - 0.5); float gridY = abs(fract(vTexCoord.y * 8.0) - 0.5); float grid = 1.0 - smoothstep(0.0, 0.08, min(gridX, gridY)); glow += neonColor * grid * 0.5; color = interior + glow + emissive * 2.0; }
    if (uFogDensity > 0.0) { float fogDist = length(vFragPos - uViewPos); float fogFactor = exp(-fogDist * uFogDensity); vec3 fogColor = mix(uSkyColor, vec3(0.7, 0.75, 0.8), 0.5); color = mix(fogColor, color, clamp(fogFactor, 0.0, 1.0)); }
    FragColor = vec4(color, 1.0);
}
)GLSL";

inline const char* UNLIT3D_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec2 vTexCoord;
void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)GLSL";

inline const char* UNLIT3D_FRAG = R"GLSL(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

} // namespace fallback

// ════════════════════════════════════════════════════════════════
// API publica — Punteros a los shaders activos (archivo o fallback)
// ════════════════════════════════════════════════════════════════

inline const char* FLAT2D_VERT = fallback::FLAT2D_VERT;
inline const char* FLAT2D_FRAG = fallback::FLAT2D_FRAG;
inline const char* DEPTH_VERT  = fallback::DEPTH_VERT;
inline const char* DEPTH_FRAG  = fallback::DEPTH_FRAG;
inline const char* PBR_VERT    = fallback::PBR_VERT;
inline const char* PBR_FRAG    = fallback::PBR_FRAG;
inline const char* UNLIT3D_VERT = fallback::UNLIT3D_VERT;
inline const char* UNLIT3D_FRAG = fallback::UNLIT3D_FRAG;

/// Inicializar ShaderLibrary: intenta cargar desde assets/shaders/.
/// Si los archivos no existen, usa los fallbacks embebidos.
/// Llamar ANTES de compilar cualquier shader.
inline void init(const char* shaderDir = "assets/shaders/") {
    if (detail::s_initialized) return;

    // Intentar cargar cada par desde disco
    if (ShaderLoader::loadPair(shaderDir, "flat2d", detail::s_flat2dVert, detail::s_flat2dFrag)) {
        FLAT2D_VERT = detail::s_flat2dVert.c_str();
        FLAT2D_FRAG = detail::s_flat2dFrag.c_str();
    }

    if (ShaderLoader::loadPair(shaderDir, "depth", detail::s_depthVert, detail::s_depthFrag)) {
        DEPTH_VERT = detail::s_depthVert.c_str();
        DEPTH_FRAG = detail::s_depthFrag.c_str();
    }

    if (ShaderLoader::loadPair(shaderDir, "pbr", detail::s_pbrVert, detail::s_pbrFrag)) {
        PBR_VERT    = detail::s_pbrVert.c_str();
        PBR_FRAG    = detail::s_pbrFrag.c_str();
    }

    if (ShaderLoader::loadPair(shaderDir, "unlit3d", detail::s_unlit3dVert, detail::s_unlit3dFrag)) {
        UNLIT3D_VERT = detail::s_unlit3dVert.c_str();
        UNLIT3D_FRAG = detail::s_unlit3dFrag.c_str();
    }

    detail::s_initialized = true;
    std::cout << "[ShaderLibrary] Inicializado (dir: " << shaderDir << ")" << std::endl;
}

} // namespace ShaderLibrary
} // namespace renderer
} // namespace engine
