#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 colorOut;

layout(scalar, buffer_reference) buffer VertexBuffer {
vec3 vertices[];
};

layout(scalar, buffer_reference) buffer FaceIndexBuffer {
vec3 colors[];
};

layout(scalar, push_constant) uniform T {
VertexBuffer vertex_buffer;
mat4 matrix;
float time;
FaceIndexBuffer face_index_buffer;
}
push_constants;

void main() {
vec3 c2 = vec3(push_constants.face_index_buffer.colors[gl_PrimitiveID]);

colorOut = vec4(c2, 1.0);
}