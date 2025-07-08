#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0) uniform image2D renderTarget;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(scalar, push_constant) uniform T {
    vec2 triangle[3];
    float time;
} push_constants;

float cross2d(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

bool is_inside_edge(vec2 e0, vec2 e1, vec2 p) {
    // Geometric interpretation: cross product
    vec2 ep1 = p - e0;
    vec2 ep2 = p - e1;
    return cross2d(ep1, ep2) > 0;
    // More naive version, derived from linear slope equations (f(x) = a + bx):
    // if (e1.x == e0.x)
    //   return (e1.x > p.x) ^^ (e0.y > e1.y);
    // float a = (e1.y - e0.y) / (e1.x - e0.x);
    // float b = e0.y + (0 - e0.x) * a;
    // float ey = a * p.x + b;
    // return (ey < p.y) ^^ (e0.x > e1.x);
}

bool same_side(vec2 a, vec2 b, vec2 c, vec2 p) {
    float cp1 = cross2d(b - a, p - a);
    float cp2 = cross2d(b - a, c - a);
    return cp1 * cp2 >= 0.0;
}

void main() {
    ivec2 img_size = imageSize(renderTarget);
    if(gl_GlobalInvocationID.x >= img_size.x || gl_GlobalInvocationID.y >= img_size.y)
        return;

    uint ok = (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2;

    float t = push_constants.time;
    float r = 0.5 + 0.5 * sin(t);
    float g = 0.5 + 0.5 * sin(t + 2.094);
    float b = 0.5 + 0.5 * sin(t + 4.188);

    vec4 c = vec4(r, g, b, 1.0);
    vec2 point = vec2(gl_GlobalInvocationID.xy) / vec2(img_size);
    point = point * 2.0 - vec2(1.0);

    vec2 v0 = vec2(push_constants.triangle[0].x, push_constants.triangle[0].y * sin(t));
    vec2 v1 = vec2(push_constants.triangle[1].x, push_constants.triangle[1].y * sin(t));
    vec2 v2 = vec2(push_constants.triangle[2].x * cos(t), push_constants.triangle[2].y);

    float phi = push_constants.time;

    if(same_side(v0, v1, v2, point) &&
        same_side(v1, v2, v0, point) &&
        same_side(v2, v0, v1, point)) {
        imageStore(renderTarget, ivec2(gl_GlobalInvocationID.xy), c);
    }
}