#version 460 core

layout(location = 0) in vec2 in_pos;

layout(location = 0) out vec3 frag_dir;

layout(push_constant) uniform Camera {
  vec4 up;
  vec4 front;
  vec4 right;
} camera;

void main() {
  gl_Position = vec4(in_pos, 0.0, 1.0);
  frag_dir = vec3(camera.right) * in_pos.x + vec3(camera.up) * in_pos.y +
             vec3(camera.front);
}
