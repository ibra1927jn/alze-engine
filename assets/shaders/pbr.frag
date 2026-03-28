#version 330 core

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
#define PI 3.14159265359

in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vFragPosLightSpace[2];  // CSM: 2 cascadas
in float vViewDepth;
out vec4 FragColor;

// ── Material ────────────────────────────────────────────────
uniform vec3  uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAo;

uniform sampler2D uAlbedoTex;
uniform bool uUseAlbedoTex;
uniform sampler2D uNormalMap;
uniform bool uUseNormalMap;

// MetallicRoughness map (glTF: G=roughness, B=metallic)
uniform sampler2D uMetallicRoughnessTex;
uniform bool uUseMetallicRoughnessTex;

// Emissive
uniform vec3 uEmissiveColor;
uniform float uEmissiveIntensity;
uniform sampler2D uEmissiveTex;
uniform bool uUseEmissiveTex;

// AO map
uniform sampler2D uAoTex;
uniform bool uUseAoTex;

// Parallax occlusion mapping
uniform sampler2D uHeightMap;
uniform bool uUseHeightMap;
uniform float uParallaxScale;

// ── Directional Light ───────────────────────────────────────
uniform vec3  uDirLightDir;
uniform vec3  uDirLightColor;

// ── IBL ─────────────────────────────────────────────────────
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D   uBRDFLUT;
uniform bool uIBLEnabled;
uniform float uIBLIntensity;

// ── Hemisphere Ambient ──────────────────────────────────────
uniform vec3 uSkyColor;
uniform vec3 uGroundColor;
uniform float uAmbientIntensity;

// ── Point Lights ────────────────────────────────────────────
struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};
uniform PointLight uPointLights[MAX_POINT_LIGHTS];
uniform int uNumPointLights;

// ── Spot Lights ─────────────────────────────────────────────
struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};
uniform SpotLight uSpotLights[MAX_SPOT_LIGHTS];
uniform int uNumSpotLights;

// ── Shadow (CSM) ────────────────────────────────────────────
uniform sampler2DShadow uShadowMap[2];
uniform bool uShadowEnabled;
uniform float uCascadeSplit;  // Profundidad view-space donde termina cascada 0
uniform vec3 uViewPos;
uniform mat4 uLightSpaceMatrix[2];  // Necesario para raymarching volumetrico
uniform int uRenderMode;  // 0=PBR, 1=Toon, 2=Neon

// ════════════════════════════════════════════════════════════
// PBR BRDF Functions — Calidad produccion
// ════════════════════════════════════════════════════════════

// Specular Anti-Aliasing: reduce brillos parpadeantes en superficies con normal map
// ensanchando la rugosidad basado en las derivadas de la normal
float filterRoughness(vec3 N, float roughness) {
    vec3 dndu = dFdx(N);
    vec3 dndv = dFdy(N);
    float variance = dot(dndu, dndu) + dot(dndv, dndv);
    float kernelRoughness = min(variance * 0.5, 0.18);  // Umbral ajustado
    return clamp(sqrt(roughness * roughness + kernelRoughness), 0.0, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

// Height-correlated Smith GGX — mas preciso que la version separable
// Combina masking y shadowing con factor de correlacion
float GeometrySmithGGX(float NdotV, float NdotL, float roughness) {
    float a2 = roughness * roughness * roughness * roughness;  // a = r^2, a2 = a^2
    float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(ggxV + ggxL, 0.0001);
}

// Aproximacion Spherical Gaussian para Fresnel — mas rapida, evita pow()
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    float f = 1.0 - cosTheta;
    return F0 + (1.0 - F0) * (f * f * f * f * f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    float f = 1.0 - cosTheta;
    float f5 = f * f * f * f * f;
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * f5;
}

// Compensacion de energia multi-scatter (Fdez-Aguera 2019)
// Agrega la energia perdida por inter-reflexion en rugosidad alta
vec3 multiscatterCompensation(vec3 F0, vec3 F, float NdotV, float roughness) {
    float Eavg = 1.0 - (roughness * 0.5);  // Albedo direccional promedio aproximado
    vec3 FssEss = F;  // Contribucion single-scatter
    vec3 Fms = FssEss * FssEss * Eavg / (1.0 - FssEss * (1.0 - Eavg));  // Multi-scatter
    return vec3(1.0) + Fms / max(FssEss, vec3(0.001));
}

// ── Soft Shadows (Vogel Disk + Interleaved Gradient Noise) ──
vec2 vogelDiskSample(int sampleIndex, int samplesCount, float phi) {
    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * 2.39996323 + phi; // Angulo dorado = 2.39996323
    return vec2(r * cos(theta), r * sin(theta));
}

float interleavedGradientNoise(vec2 position_screen) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

float calcShadowCascade(sampler2DShadow shadowMap, vec4 fragPosLightSpace, vec3 N, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    // Slope-scale bias
    float cosAngle = max(dot(N, lightDir), 0.0);
    float slopeScale = sqrt(1.0 - cosAngle * cosAngle) / max(cosAngle, 0.001);
    float bias = max(0.0005 * slopeScale, 0.0002);
    float currentDepth = projCoords.z - bias;

    // Desvanecimiento en bordes
    vec2 fade = smoothstep(vec2(0.0), vec2(0.05), projCoords.xy) *
                (1.0 - smoothstep(vec2(0.95), vec2(1.0), projCoords.xy));
    float edgeFade = fade.x * fade.y;

    // Aproximacion Contact Hardening: escalar radio por profundidad de vista
    float radiusScale = mix(2.0, 4.0, vViewDepth / 50.0);
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // Vogel disk PCF con dither IGN
    float shadow = 0.0;
    const int NUM_SAMPLES = 16;
    float noise = interleavedGradientNoise(gl_FragCoord.xy);
    float spinAngle = noise * 2.0 * PI;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec2 offset = vogelDiskSample(i, NUM_SAMPLES, spinAngle);
        vec3 sc = vec3(projCoords.xy + offset * texelSize * radiusScale, currentDepth);
        shadow += 1.0 - texture(shadowMap, sc);
    }
    return (shadow / float(NUM_SAMPLES)) * edgeFade;
}

// ── Volumetric Lighting (God Rays via Raymarching) ──────────
uniform float uVolumetricIntensity;
uniform float uFogDensity;  // 0.0 = sin niebla, 0.0005 = sutil, 0.002 = denso

float calcVolumetric(vec3 rayOrigin, vec3 rayDir, float rayLength, vec3 L) {
    const int NUM_STEPS = 16;
    float stepSize = rayLength / float(NUM_STEPS);
    vec3 stepVec = rayDir * stepSize;

    // Dither posicion inicial para ocultar banding
    float noise = interleavedGradientNoise(gl_FragCoord.xy);
    vec3 currentPos = rayOrigin + stepVec * noise;

    float vLight = 0.0;
    for(int i = 0; i < NUM_STEPS; i++) {
        // Proyectar a cascada 0
        vec4 fragPosLS = uLightSpaceMatrix[0] * vec4(currentPos, 1.0);
        vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
        projCoords = projCoords * 0.5 + 0.5;

        if(projCoords.z <= 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0) {
            float shadow = texture(uShadowMap[0], vec3(projCoords.xy, projCoords.z - 0.001));
            vLight += shadow; // 1.0 = no en sombra
        }
        currentPos += stepVec;
    }

    // Funcion de fase Henyey-Greenstein (scattering hacia adelante)
    float g = 0.6;
    float cosTheta = dot(rayDir, L);
    float phase = (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0*g*cosTheta, 1.5));

    return (vLight / float(NUM_STEPS)) * phase * rayLength * 0.02; // Escala de densidad
}

/// CSM: seleccionar cascada por profundidad de vista, con blending suave en transicion
float calcShadowCSM(vec3 N, vec3 lightDir) {
    // Seleccionar cascada basado en profundidad de vista
    float blendZone = uCascadeSplit * 0.1;  // Zona de blend 10%

    if (vViewDepth < uCascadeSplit - blendZone) {
        // Completamente en cascada 0 (cerca, alto detalle)
        return calcShadowCascade(uShadowMap[0], vFragPosLightSpace[0], N, lightDir);
    }
    else if (vViewDepth < uCascadeSplit + blendZone) {
        // Zona de blend entre cascadas
        float t = (vViewDepth - (uCascadeSplit - blendZone)) / (2.0 * blendZone);
        float s0 = calcShadowCascade(uShadowMap[0], vFragPosLightSpace[0], N, lightDir);
        float s1 = calcShadowCascade(uShadowMap[1], vFragPosLightSpace[1], N, lightDir);
        return mix(s0, s1, t);
    }
    else {
        // Cascada 1 (lejos)
        return calcShadowCascade(uShadowMap[1], vFragPosLightSpace[1], N, lightDir);
    }
}

// ── Normal Mapping (TBN por derivadas) ─────────────────────────
vec3 getNormal() {
    vec3 N = normalize(vNormal);
    if (!uUseNormalMap) return N;

    vec3 pos_dx = dFdx(vFragPos);
    vec3 pos_dy = dFdy(vFragPos);
    vec2 tex_dx = dFdx(vTexCoord);
    vec2 tex_dy = dFdy(vTexCoord);

    vec3 T = normalize(pos_dx * tex_dy.y - pos_dy * tex_dx.y);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    vec3 tangentNormal = texture(uNormalMap, vTexCoord).xyz * 2.0 - 1.0;
    return normalize(TBN * tangentNormal);
}

// ── Parallax Occlusion Mapping ─────────────────────────────
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) {
    // Cantidad adaptiva de pasos: mas pasos en angulos rasantes
    float numSteps = mix(8.0, 32.0, abs(dot(vec3(0,0,1), viewDir)));
    float layerDepth = 1.0 / numSteps;
    float currentLayerDepth = 0.0;
    vec2 P = viewDir.xy / viewDir.z * uParallaxScale;
    vec2 deltaTexCoords = P / numSteps;

    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(uHeightMap, currentTexCoords).r;

    while (currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(uHeightMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    // Interpolacion para resultado suave
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(uHeightMap, prevTexCoords).r - currentLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentTexCoords, prevTexCoords, weight);
}

// ── Calculo de luz PBR (calidad produccion) ──────────────
vec3 calcPBRLight(vec3 L, vec3 lightColor, vec3 N, vec3 V,
                   vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmithGGX(NdotV, NdotL, roughness);  // Height-correlated
    vec3  F   = fresnelSchlick(HdotV, F0);

    // Especular (G ya incluye 1/(4*NdotV*NdotL) via forma height-correlated)
    vec3 specular = NDF * G * F;

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    // Compensacion de energia multi-scatter
    vec3 compensation = multiscatterCompensation(F0, F, NdotV, roughness);
    specular *= compensation;

    return (kD * albedo / PI + specular) * lightColor * NdotL;
}

void main() {
    // Offset UV por parallax si hay height map presente
    vec2 texCoord = vTexCoord;
    if (uUseHeightMap) {
        // Construir TBN para parallax (mismo que getNormal)
        vec3 pos_dx = dFdx(vFragPos);
        vec3 pos_dy = dFdy(vFragPos);
        vec2 tex_dx = dFdx(vTexCoord);
        vec2 tex_dy = dFdy(vTexCoord);
        vec3 T = normalize(pos_dx * tex_dy.y - pos_dy * tex_dx.y);
        vec3 B = normalize(cross(normalize(vNormal), T));
        vec3 N = normalize(vNormal);
        mat3 TBN = mat3(T, B, N);
        vec3 viewDirTangent = normalize(transpose(TBN) * (uViewPos - vFragPos));
        texCoord = parallaxMapping(vTexCoord, viewDirTangent);
        // Descartar fragmentos fuera del rango UV [0,1]
        if (texCoord.x > 1.0 || texCoord.y > 1.0 || texCoord.x < 0.0 || texCoord.y < 0.0)
            discard;
    }

    // Albedo (texturas sRGB auto-linealizadas por hardware con GL_SRGB8)
    vec3 albedo = uAlbedo;
    if (uUseAlbedoTex)
        albedo *= texture(uAlbedoTex, texCoord).rgb;

    // Metallic + Roughness (desde textura o escalar)
    float metallic  = uMetallic;
    float roughness = uRoughness;
    if (uUseMetallicRoughnessTex) {
        vec4 mr = texture(uMetallicRoughnessTex, texCoord);
        roughness *= mr.g;  // glTF: verde = roughness
        metallic  *= mr.b;  // glTF: azul  = metallic
    }
    roughness = max(roughness, 0.04);

    // AO (desde textura o escalar)
    float ao = uAo;
    if (uUseAoTex)
        ao *= texture(uAoTex, texCoord).r;

    vec3 N = getNormal();
    vec3 V = normalize(uViewPos - vFragPos);

    // Specular Anti-Aliasing: ensanchar roughness en normales de alta frecuencia
    roughness = filterRoughness(N, roughness);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── Shadow (CSM) ──────────────────────────────────────
    float shadow = 0.0;
    vec3 lightDir = normalize(uDirLightDir);
    if (uShadowEnabled)
        shadow = calcShadowCSM(N, lightDir);

    // ── Directional Light ───────────────────────────────────
    vec3 Lo = calcPBRLight(lightDir, uDirLightColor, N, V,
                           albedo, metallic, roughness, F0);
    Lo *= (1.0 - shadow * 0.85);  // Sombras nunca completamente negras (15% luz minima)

    // God Rays volumetricos
    if (uShadowEnabled && uVolumetricIntensity > 0.0) {
        float rayLength = length(vFragPos - uViewPos);
        vec3 rayDir = (vFragPos - uViewPos) / rayLength;
        // Limitar longitud maxima del rayo por rendimiento y para mantener scattering localizado
        float vLight = calcVolumetric(uViewPos, rayDir, min(rayLength, uCascadeSplit), lightDir);
        Lo += uDirLightColor * vLight * uVolumetricIntensity;
    }

    // ── Point Lights ────────────────────────────────────────
    for (int i = 0; i < uNumPointLights; i++) {
        vec3 lightVec = uPointLights[i].position - vFragPos;
        float dist = length(lightVec);
        vec3 L = lightVec / dist;
        float attenuation = 1.0 / (uPointLights[i].constant +
                                    uPointLights[i].linear * dist +
                                    uPointLights[i].quadratic * dist * dist);
        vec3 radiance = uPointLights[i].color * attenuation;
        Lo += calcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // ── Spot Lights ─────────────────────────────────────────
    for (int i = 0; i < uNumSpotLights; i++) {
        vec3 lightVec = uSpotLights[i].position - vFragPos;
        float dist = length(lightVec);
        vec3 L = lightVec / dist;
        float theta = dot(L, normalize(-uSpotLights[i].direction));
        float epsilon = uSpotLights[i].cutOff - uSpotLights[i].outerCutOff;
        float intensity = clamp((theta - uSpotLights[i].outerCutOff) / epsilon, 0.0, 1.0);
        float attenuation = 1.0 / (uSpotLights[i].constant +
                                    uSpotLights[i].linear * dist +
                                    uSpotLights[i].quadratic * dist * dist);
        vec3 radiance = uSpotLights[i].color * attenuation * intensity;
        Lo += calcPBRLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // ── Ambient (IBL o hemisphere fallback) ──────────────────
    float NdotV = max(dot(N, V), 0.0);
    vec3 kS = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    vec3 ambient;
    if (uIBLEnabled) {
        // IBL difuso: cubemap de irradiancia
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuseIBL = kD * albedo * irradiance;

        // IBL especular: env pre-filtrado + BRDF LUT
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(uBRDFLUT, vec2(NdotV, roughness)).rg;
        vec3 specularIBL = prefilteredColor * (kS * brdf.x + brdf.y);

        ambient = (diffuseIBL + specularIBL) * ao * uIBLIntensity;
    } else {
        // Hemisphere fallback
        float hemisphere = N.y * 0.5 + 0.5;
        vec3 ambientColor = mix(uGroundColor, uSkyColor, hemisphere);
        vec3 R = reflect(-V, N);
        float envHemi = R.y * 0.5 + 0.5;
        vec3 envColor = mix(uGroundColor * 2.0, uSkyColor * 4.0, envHemi);
        vec3 diffuseAmbient = kD * albedo * ambientColor;
        float smoothness = 1.0 - roughness * roughness;
        vec3 specEnv = mix(ambientColor, envColor, smoothness);
        vec3 specularAmbient = kS * specEnv;
        ambient = (diffuseAmbient + specularAmbient) * uAmbientIntensity * ao;
    }

    // ── Emissive ─────────────────────────────────────────────
    vec3 emissive = uEmissiveColor * uEmissiveIntensity;
    if (uUseEmissiveTex)
        emissive *= texture(uEmissiveTex, vTexCoord).rgb;

    // ── Color final (PBR por defecto) ──────────────────────────────
    vec3 color = ambient + Lo + emissive;

    // ── Procesamiento por modo de render ──────────────────────────────
    if (uRenderMode == 1) {
        // TOON CEL-SHADING
        float NdotL_toon = max(dot(N, lightDir), 0.0);
        // Cuantizar en 4 bandas
        float toonLight;
        if (NdotL_toon > 0.8) toonLight = 1.0;
        else if (NdotL_toon > 0.4) toonLight = 0.7;
        else if (NdotL_toon > 0.15) toonLight = 0.4;
        else toonLight = 0.2;

        // Deteccion de bordes (silueta outline)
        float edgeFactor = dot(N, V);
        float edge = smoothstep(0.0, 0.25, edgeFactor);

        // Boost de saturacion del albedo para look cartoon
        float lum = dot(albedo, vec3(0.299, 0.587, 0.114));
        vec3 saturated = mix(vec3(lum), albedo, 1.5);

        vec3 toonColor = saturated * toonLight * uDirLightColor;
        toonColor += emissive;
        toonColor *= edge; // Outlines negros
        color = toonColor;
    }
    else if (uRenderMode == 2) {
        // NEON HOLOGRAPHIC
        float fresnel = 1.0 - max(dot(N, V), 0.0);
        fresnel = pow(fresnel, 2.0);

        // Glow neon de bordes con color tintado por albedo
        vec3 neonColor = mix(albedo * 2.0, vec3(0.4, 0.8, 1.0), 0.3);
        vec3 glow = neonColor * fresnel * 3.0;

        // Interior oscuro con respuesta sutil a la luz
        float NdotL_neon = max(dot(N, lightDir), 0.0);
        vec3 interior = albedo * 0.05 * (0.5 + NdotL_neon * 0.5);

        // Lineas de grid estilo wireframe desde UV
        float gridX = abs(fract(vTexCoord.x * 8.0) - 0.5);
        float gridY = abs(fract(vTexCoord.y * 8.0) - 0.5);
        float grid = 1.0 - smoothstep(0.0, 0.08, min(gridX, gridY));
        glow += neonColor * grid * 0.5;

        color = interior + glow + emissive * 2.0;
    }

    // ── Niebla atmosferica ──────────────────────────────────
    if (uFogDensity > 0.0) {
        float fogDist = length(vFragPos - uViewPos);
        float fogFactor = exp(-fogDist * uFogDensity);
        vec3 fogColor = mix(uSkyColor, vec3(0.7, 0.75, 0.8), 0.5);
        color = mix(fogColor, color, clamp(fogFactor, 0.0, 1.0));
    }

    FragColor = vec4(color, 1.0);
}
