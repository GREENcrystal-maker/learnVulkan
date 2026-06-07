#version 450

struct DirectionalLight {vec3 dir; vec3 color;float intensity;};
struct PointLight {vec3 pos;vec3 color;vec2 args;};
struct SpotLight {vec3 pos;vec3 dir;vec3 color;vec4 args; };
layout(binding = 0) uniform UniformBufferObject {
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


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;


vec3 CalcBlinnPhong(vec3 norm, vec3 lightDir, vec3 viewDir, vec3 ambient,vec3 materialColor,vec3 lightColor,float intensity, float p1,float p2) {//根据p238公式10.9//ambient是（r,g,b）*他的强度，对应公式里ca//两color对应cr cp//光强对应cl
    vec3 halfDir = normalize(lightDir + viewDir);
    vec3 diff = materialColor*pow(max(dot(norm, lightDir), 0.0), p1);
    vec3 spec = lightColor*pow(dot(norm, halfDir), p2);
    return intensity * (diff + spec); + materialColor*ambient;
}

// ================= 1. 方向光计算 =================
vec3 CalcDirLight(DirectionalLight light, vec3 norm, vec3 viewDir,vec3 ambient,vec3 materialColor,vec2 strength) {
    vec3 lightDir = normalize(-light.dir); 
    return CalcBlinnPhong(norm, lightDir, viewDir, ambient, materialColor, light.color, light.intensity, strength.x, strength.y);
}

vec3 CalcPointLight(PointLight light, vec3 norm, vec3 viewDir,vec3 ambient,vec3 materialColor,vec2 strength) {//参数同上，灯类换成点光源//用上args（实际上，args的第一个传1.0，第二个填一个本lab达不到的很大值，所以没什么用，没有也行)
    float distance = length(light.pos - fragPos);
    if(distance > light.args.y) return ambient*materialColor;//如果距离大于衰减范围，直接返回环境光
    vec3 lightDir = normalize(light.pos - fragPos);//物点指向灯点的向量
    float intensity=light.args.x*(1/(1+distance*distance));
    return CalcBlinnPhong(norm, lightDir, viewDir, ambient, materialColor, light.color, intensity, strength.x, strength.y);
}

//聚光灯逻辑:只有intensity项不同于pointlight，内锥角里intensity是1，外锥角外是0，过渡区根据夹角线性插值
vec3 CalcSpotLight(SpotLight light, vec3 norm, vec3 viewDir,vec3 ambient,vec3 materialColor,vec2 strength) {
    float distance = length(light.pos - fragPos);
    if(distance > light.args.w) return ambient*materialColor;//如果距离大于衰减范围，直接返回环境光
    vec3 lightDir = normalize(light.pos - fragPos);
    vec3 spotDir = normalize(light.dir);
    //处理intensity
    float costheta = dot(lightDir, -spotDir); // 注意这里用 -spotDir 因为 lightDir 是指向光源的//注意cos是递减函数
    float cutOff = light.args.x;      // cos(内锥角)
    float outerCutOff = light.args.y; // cos(外锥角)
    // 平滑过渡
    float epsilon = cutOff - outerCutOff;
    float intensity0 = clamp((costheta - outerCutOff) / epsilon, 0.0, 1.0);//包含了在内锥角里和外锥角外的情况

    float intensity=light.args.z*(1/(1+distance*distance))*intensity0;
    return CalcBlinnPhong(norm, lightDir, viewDir, ambient, materialColor, light.color, intensity, strength.x, strength.y);
}


void main() {
    vec3 norm = normalize(normal);
    vec3 viewDir = normalize(ubo.viewPos-fragPos);
    vec3 result =vec3(texture(texSampler, fragTexCoord));
    vec3 ambient=vec3(ubo.ambientArgs)*ubo.ambientArgs.w;//ca

    for(int i = 0; i < ubo.lightCounts.x; ++i) {
        result += CalcDirLight(ubo.dirLights[i], norm, viewDir,ambient,result,ubo.strength);
    }
    // 累加点光源
    for(int i = 0; i < ubo.lightCounts.y; ++i) {
        result += CalcPointLight(ubo.pointLights[i], norm, viewDir,ambient,result,ubo.strength);
    }
    // 累加聚光灯
    for(int i = 0; i < ubo.lightCounts.z; ++i) {
        result += CalcSpotLight(ubo.spotLights[i], norm, viewDir,ambient,result,ubo.strength);
    }
    outColor = vec4(result, 1.0);
}