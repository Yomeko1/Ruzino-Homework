#version 430 core

layout(location = 0) in vec3 aPos;

uniform mat4 light_view;
uniform mat4 light_projection;
uniform mat4 model;

out vec3 vertexPosition;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vec4 lightViewPos = light_view * worldPos;
    vertexPosition = worldPos.xyz;
    gl_Position = light_projection * lightViewPos;
}