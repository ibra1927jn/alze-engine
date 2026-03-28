#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform mat4 uLightSpaceMatrix[2];  // CSM: 2 cascades

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vFragPosLightSpace[2];  // CSM: 2 cascade light positions
out float vViewDepth;             // Para seleccion de cascada

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    vFragPosLightSpace[0] = uLightSpaceMatrix[0] * worldPos;
    vFragPosLightSpace[1] = uLightSpaceMatrix[1] * worldPos;
    vec4 viewPos = uView * worldPos;
    vViewDepth = -viewPos.z;  // Profundidad positiva hacia la pantalla
    gl_Position = uProjection * viewPos;
}
