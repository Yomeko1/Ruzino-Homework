#version 430

layout(location = 0) out vec3 position;
layout(location = 1) out float depth;
layout(location = 2) out vec2 texcoords;
layout(location = 3) out vec3 diffuseColor;
layout(location = 4) out vec2 metallicRoughness;
layout(location = 5) out vec3 normal;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vTexcoord;
uniform mat4 projection;
uniform mat4 view;

uniform sampler2D diffuseColorSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D metallicRoughnessSampler;

void main() {
    position = vertexPosition;
    vec4 clipPos = projection * view * vec4(position, 1.0);
    depth = clipPos.z / clipPos.w;
    texcoords = vTexcoord;

    diffuseColor = texture(diffuseColorSampler, vTexcoord).xyz;
    metallicRoughness = texture(metallicRoughnessSampler, vTexcoord).zy;

    vec3 N = normalize(vertexNormal);

    vec3 normalmap_value = texture(normalMapSampler, vTexcoord).xyz;
    normalmap_value = normalmap_value * 2.0 - 1.0;

    vec3 edge1 = dFdx(vertexPosition);
    vec3 edge2 = dFdy(vertexPosition);
    vec2 deltaUV1 = dFdx(vTexcoord);
    vec2 deltaUV2 = dFdy(vTexcoord);

    vec3 tangent = edge1 * deltaUV2.y - edge2 * deltaUV1.y;

    if(length(tangent) < 1E-7) {
        vec3 bitangent = -edge1 * deltaUV2.x + edge2 * deltaUV1.x;
        tangent = normalize(cross(bitangent, N));
    }
    tangent = normalize(tangent - dot(tangent, N) * N);
    vec3 bitangent = normalize(cross(tangent, N));

    mat3 TBN = mat3(tangent, bitangent, N);
    normal = normalize(TBN * normalmap_value);
}