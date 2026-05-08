#version 430 core

uniform mat4 light_view;
uniform mat4 light_projection;
in vec3 vertexPosition;
layout(location = 0) out vec4 shadow_map0;

void main() {
    vec4 clipPos = light_projection * light_view * (vec4(vertexPosition, 1.0));
    float linearDepth = length((light_view * vec4(vertexPosition, 1.0)).xyz);
    shadow_map0 = vec4(linearDepth / 25.0, 0.0, 0.0, 1.0);
}