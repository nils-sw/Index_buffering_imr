#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 color;

layout(scalar, buffer_reference) buffer VertexBuffer {
vec3 vertices[];
};

layout(scalar, buffer_reference) buffer IndexBuffer {
uint indices[36];
};

layout(scalar, buffer_reference) buffer FaceIndexBuffer {
uint face_indices[12];
};

layout(scalar, push_constant) uniform T {
VertexBuffer vertex_buffer;
mat4 matrix;
float time;
    //IndexBuffer index_buffer;
    //FaceIndexBuffer face_index_buffer;
}
push_constants;

void main() {
const int VERTEX_COUNT = 8;
const int FACE_COLOR_OFFSET = VERTEX_COUNT;
    //uint idx = push_constants.index_buffer.indices[gl_VertexIndex];
    //uint tri = gl_VertexIndex / 3;
    //uint face_idx = push_constants.face_index_buffer.face_indices[tri];
vec3 vertex = push_constants.vertex_buffer.vertices[gl_VertexIndex];
color = vec3(1, 0, 0); //push_constants.vertex_buffer.vertices[FACE_COLOR_OFFSET + face_idx];
gl_Position = push_constants.matrix * vec4(vertex, 1.0);
}