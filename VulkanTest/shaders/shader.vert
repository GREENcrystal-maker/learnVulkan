#version 450


struct DirectionalLight {vec3 dir; vec3 color;float intensity;};
struct PointLight {vec3 pos;vec3 color;vec2 args;};
struct SpotLight {vec3 pos;vec3 dir;vec3 color;vec4 args; };
layout(binding = 0, std140) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    DirectionalLight dirLights[1];
    PointLight pointLights[2];
    SpotLight spotLights[1];

    ivec3 lightCounts;
    vec4 ambientArgs;
    vec2 strength;
    vec3 viewPos;
    mat4 normalMatrix;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;       // 【新增】世界空间中的顶点位置
layout(location = 3) out vec3 Normal;



void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragPos = vec3(ubo.model * vec4(inPosition, 1.0));
    Normal = mat3(ubo.normalMatrix) * inNormal;//更新法线
}