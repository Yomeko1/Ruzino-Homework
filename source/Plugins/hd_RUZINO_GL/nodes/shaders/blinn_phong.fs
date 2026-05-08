#version 430 core

struct Light {
    mat4 light_projection;
    mat4 light_view;
    vec3 position;
    float radius;
    vec3 color;
    int shadow_map_id;
};

layout(binding = 0) buffer lightsBuffer {
Light lights[4];
};

uniform vec2 iResolution;

uniform sampler2D diffuseColorSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D metallicRoughnessSampler;
uniform sampler2DArray shadow_maps;
uniform sampler2D positionSampler;

uniform vec3 camPos;
uniform int light_count;

uniform float ambientStrength = 0.1;

layout(location = 0) out vec4 Color;

void main() {
vec2 uv = gl_FragCoord.xy / iResolution;

vec3 worldPos = texture(positionSampler, uv).xyz;
vec3 normal = texture(normalMapSampler, uv).xyz;
vec3 diffuseColor = texture(diffuseColorSampler, uv).xyz;

vec4 metalnessRoughness = texture(metallicRoughnessSampler, uv);
float metal = metalnessRoughness.x;
float roughness = metalnessRoughness.y;

vec3 ambient = ambientStrength * diffuseColor * vec3(1.0);
vec3 result = ambient;

for(int i = 0; i < light_count; i ++) {

vec3 lightDir = lights[i].position - worldPos;
float lightDist = length(lightDir);
vec3 L = lightDir / lightDist;

float attenuation = 1.0 / (1.0 + 0.09 * lightDist + 0.032 * lightDist * lightDist);

// Shadow disabled for testing
float shadow = 1.0;

vec3 V = normalize(camPos - worldPos);
vec3 N = normalize(normal);
vec3 H = normalize(L + V);

float shininess = (1.0 - roughness) * 256.0;

float NdotL = max(dot(N, L), 0.0);
float NdotH = max(dot(N, H), 0.0);

float ks = pow(NdotH, shininess);
float kd = 1.0 - ks;

kd *= (1.0 - metal);

vec3 diffuse = kd * diffuseColor * lights[i].color * NdotL;
vec3 specular = ks * lights[i].color * NdotH;

result += (diffuse + specular) * attenuation * shadow;
}

Color = vec4(result, 1.0);

}