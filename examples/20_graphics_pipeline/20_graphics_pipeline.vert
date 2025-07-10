#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 color;

layout(scalar, buffer_reference) buffer VertexBuffer {
vec3 vertices[];
};

layout(scalar, buffer_reference) buffer FaceBuffer {
vec3 color[];
};

layout(scalar, buffer_reference) buffer PositionBuffer {
vec3 positions[];
};

layout(scalar, push_constant) uniform T {
VertexBuffer vertex_buffer;
mat4 matrix;
float time;
FaceBuffer face_index_buffer;
PositionBuffer position_buffer;
}
push_constants;

void main() {
vec3 vertex = push_constants.position_buffer.positions[gl_InstanceIndex] + push_constants.vertex_buffer.vertices[gl_VertexIndex];
gl_Position = push_constants.matrix * vec4(vertex, 1.0);
}