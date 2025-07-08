#include "imr/imr.h"
#include "imr/util.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "../common/camera.h"

using namespace nasl;

struct Tri
{
    vec3 v0, v1, v2;
    vec3 color;
};

struct Face
{
    vec3 v0, v1, v2, v3;
    vec3 color;
};

struct Cube
{
    Tri triangles[12];
    Face faces[6];
};

struct FaceIdx
{
    uint32_t v0, v1, v2, v3;
    vec3 color;
};

Cube make_cube()
{
    /*
     *  +Y
     *  ^
     *  |
     *  |
     *  D------C.
     *  |\     |\
     *  | H----+-G
     *  | |    | |
     *  A-+----B | ---> +X
     *   \|     \|
     *    E------F
     *     \
     *      \
     *       \
     *        v +Z
     *
     * Adapted from
     * https://www.asciiart.eu/art-and-design/geometries
     */
    vec3 A = {0, 0, 0};
    vec3 B = {1, 0, 0};
    vec3 C = {1, 1, 0};
    vec3 D = {0, 1, 0};
    vec3 E = {0, 0, 1};
    vec3 F = {1, 0, 1};
    vec3 G = {1, 1, 1};
    vec3 H = {0, 1, 1};

    int i = 0;
    Cube cube = {};

    auto add_face = [&](vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec3 color)
    {
        /*
         * v0 --- v3
         *  |   / |
         *  |  /  |
         *  | /   |
         * v1 --- v2
         */
        cube.triangles[i++] = {v0, v1, v3, color};
        cube.triangles[i++] = {v1, v2, v3, color};
        cube.faces[i++] = {v0, v1, v2, v3, color};
    };

    // top face
    add_face(H, D, C, G, vec3(0, 1, 0));
    // north face
    add_face(A, B, C, D, vec3(1, 0, 0));
    // west face
    add_face(A, D, H, E, vec3(0, 0, 1));
    // east face
    add_face(F, G, C, B, vec3(1, 0, 1));
    // south face
    add_face(E, H, G, F, vec3(0, 1, 1));
    // bottom face
    add_face(E, F, B, A, vec3(1, 1, 0));
    assert(i == 12 + 6);
    return cube;
}

struct
{
    VkDeviceAddress vertex_buffer;
    mat4 matrix;
    float time;
    // VkDeviceAddress index_buffer;
    // VkDeviceAddress face_index_buffer;
} push_constants_batched;

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow *, CameraInput *input);

bool reload_shaders = false;

#define INSTANCES_COUNT 1

struct Shaders
{
    std::vector<std::string> files = {"20_graphics_pipeline.vert.spv", "20_graphics_pipeline.frag.spv"};

    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> pipeline;

    Shaders(imr::Device &d, imr::Swapchain &swapchain)
    {
        imr::GraphicsPipeline::RenderTargetsState rts;
        rts.color.push_back((imr::GraphicsPipeline::RenderTarget){
            .format = swapchain.format(),
            .blending = {
                .blendEnable = false,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}});
        imr::GraphicsPipeline::RenderTarget depth = {
            .format = VK_FORMAT_D32_SFLOAT};
        rts.depth = depth;

        imr::GraphicsPipeline::StateBuilder stateBuilder = {
            .vertexInputState = imr::GraphicsPipeline::no_vertex_input(),
            .inputAssemblyState = imr::GraphicsPipeline::simple_triangle_input_assembly(),
            .viewportState = imr::GraphicsPipeline::one_dynamically_sized_viewport(),
            .rasterizationState = imr::GraphicsPipeline::solid_filled_polygons(),
            .multisampleState = imr::GraphicsPipeline::one_spp(),
            .depthStencilState = imr::GraphicsPipeline::simple_depth_testing(),
        };

        std::vector<imr::ShaderEntryPoint *> entry_point_ptrs;
        for (auto filename : files)
        {
            VkShaderStageFlagBits stage;
            if (filename.ends_with("vert.spv"))
                stage = VK_SHADER_STAGE_VERTEX_BIT;
            else if (filename.ends_with("frag.spv"))
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            else
                throw std::runtime_error("Unknown suffix");
            modules.push_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            entry_points.push_back(std::make_unique<imr::ShaderEntryPoint>(*modules.back(), stage, "main"));
            entry_point_ptrs.push_back(entry_points.back().get());
        }
        pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
    }
};

int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
                       {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true; });

    imr::Context context;
    imr::Device device(context);
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    auto cube = make_cube();
    std::unique_ptr<imr::Buffer> vertex_buffer;
    std::unique_ptr<imr::Buffer> index_buffer;
    std::vector<uint32_t> indices;

    // if (true)
    // {
    std::vector<vec3> vertices = {
        {0, 0, 0}, // 0: A
        {1, 0, 0}, // 1: B
        {1, 1, 0}, // 2: C
        {0, 1, 0}, // 3: D
        {0, 0, 1}, // 4: E
        {1, 0, 1}, // 5: F
        {1, 1, 1}, // 6: G
        {0, 1, 1}  // 7: H
    };
    // Use per-face colors
    std::vector<vec3> faceColors = {
        {0, 1, 0}, // top
        {1, 0, 0}, // north
        {0, 0, 1}, // west
        {1, 0, 1}, // east
        {0, 1, 1}, // south
        {1, 1, 0}  // bottom
    };
    std::vector<FaceIdx> faces = {
        // top face (H, D, C, G)
        {7, 3, 2, 6, faceColors[0]},
        // north face (A, B, C, D)
        {0, 1, 2, 3, faceColors[1]},
        // west face (A, D, H, E)
        {0, 3, 7, 4, faceColors[2]},
        // east face (F, G, C, B)
        {5, 6, 2, 1, faceColors[3]},
        // south face (E, H, G, F)
        {4, 7, 6, 5, faceColors[4]},
        // bottom face (E, F, B, A)
        {4, 5, 1, 0, faceColors[5]}};
    std::vector<uint32_t> face_indices; // For each triangle, which face it belongs to
    for (size_t f = 0; f < faces.size(); ++f)
    {
        // First triangle: v0, v1, v3
        indices.push_back(faces[f].v0);
        indices.push_back(faces[f].v1);
        indices.push_back(faces[f].v3);
        face_indices.push_back(f);
        // Second triangle: v1, v2, v3
        indices.push_back(faces[f].v1);
        indices.push_back(faces[f].v2);
        indices.push_back(faces[f].v3);
        face_indices.push_back(f);
    }

    // Create a single buffer for positions (8) and face colors (6)
    std::vector<vec3> vertex_buffer_data;
    std::vector<uint32_t> index_buffer_data;
    vertex_buffer_data.insert(vertex_buffer_data.end(), vertices.begin(), vertices.end());
    vertex_buffer_data.insert(vertex_buffer_data.end(), faceColors.begin(), faceColors.end());
    index_buffer_data.insert(index_buffer_data.end(), indices.begin(), indices.end());

    vertex_buffer = std::make_unique<imr::Buffer>(device, sizeof(vertex_buffer_data[0]) * vertex_buffer_data.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    index_buffer = std::make_unique<imr::Buffer>(device, sizeof(index_buffer_data[0]) * index_buffer_data.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    push_constants_batched.vertex_buffer = vertex_buffer->device_address();
    // push_constants_batched.index_buffer = index_buffer->device_address();
    vertex_buffer->uploadDataSync(0, vertex_buffer->size, vertex_buffer_data.data());
    index_buffer->uploadDataSync(0, index_buffer->size, index_buffer_data.data());

    auto face_index_buffer = std::make_unique<imr::Buffer>(
        device,
        sizeof(uint32_t) * face_indices.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    face_index_buffer->uploadDataSync(0, face_index_buffer->size, face_indices.data());
    // push_constants_batched.face_index_buffer = face_index_buffer->device_address();

    for (size_t i = 0; i < indices.size(); ++i)
    {
        printf("%u ", indices[i]);
        if ((i + 1) % 6 == 0)
            printf("\n");
    }
    // }

    std::vector<vec3> positions;
    for (size_t i = 0; i < INSTANCES_COUNT; i++)
    {
        vec3 p;
        p.x = ((float)rand() / RAND_MAX) * 20 - 10;
        p.y = ((float)rand() / RAND_MAX) * 20 - 10;
        p.z = ((float)rand() / RAND_MAX) * 20 - 10;
        positions.push_back(p);
    }

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    auto shaders = std::make_unique<Shaders>(device, swapchain);

    auto &vk = device.dispatch;
    while (!glfwWindowShouldClose(window))
    {
        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext &context)
                                        {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(device, swapchain);
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = 0,
                        .srcAccessMask = 0,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .image = depthBuffer->handle(),
                        .subresourceRange = depthBuffer->whole_image_subresource_range()
                    })
                }));
            }

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
                .depth = 1.0f,
                .stencil = 0,
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // This barrier ensures that the clear is finished before we run the dispatch.
            // before: all writes from the "transfer" stage (to which the clear command belongs)
            // after: all writes from the "compute" stage
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                })
            }));

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            auto& pipeline = shaders->pipeline;
            
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());
            vkCmdBindIndexBuffer(cmdbuf, index_buffer->handle, 0, VK_INDEX_TYPE_UINT32);

            push_constants_batched.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

            context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                for (auto pos : positions) {
                    mat4 cube_matrix = m;
                    cube_matrix = cube_matrix * translate_mat4(pos);

                    //Work here for the index buffer: vkCmdDrawIndexed and add the index buffer to the pipeline
                    push_constants_batched.matrix = cube_matrix;
                    vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_batched), &push_constants_batched);
                    
                    vkCmdDrawIndexed(cmdbuf, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
                }
            });

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents(); });
    }

    swapchain.drain();
    return 0;
}
