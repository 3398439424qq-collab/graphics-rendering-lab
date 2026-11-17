// 程序入口：串联各模块完成 Vulkan 初始化与三角形渲染，每个阶段打印一行日志，最终用退出码报告首个失败阶段。
// 运行方式：无参数 = 只枚举设备并打印；--window = 开窗循环绘制三角形直到关闭；--maintenance-check = 检查 swapchain_maintenance1 是否可用。
#include "surface.h" 
#include "window.h" 
#include "device.h"
#include "instance.h"
#include "swapchain.h"
#include "image_views.h"
#include "render_targets.h"
#include "shader_modules.h"
#include "frame_presentation.h"
#include "graphics_pipeline.h"
#include "surface_options.h"
#include "swapchain_choices.h"
#include "surface_capabilities.h"
#include "command_recording.h"
#include "enumeration.h"
#include "device_info.h"
#include "queue_families.h"
#include "surface_support.h"
#include "queue_family_selection.h"
#include "device_selection.h"
#include "swapchain_generation.h"
#include "buffer_resources.h"
#include "buffer_upload.h"
#include "image_resources.h"
#include "image_upload.h"
#include "texture_data.h"
#include "texture_binding.h"
#include "mesh_loading.h"
#include "transform_math.h"
#include "scene_config.h"
#include "material_uniform.h"
#include "material_binding.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


#include <cstdint>
#include <cstring>
#include <iomanip>
#include <vector>
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <cmath>
#include <limits>

namespace {
    // ---- 提前声明已有私有辅助函数，保证 run_window_session 中可见 ----
    void print_window_error(const char* stage, const std::string& error);
    const char* surface_list_status_name(SurfaceListStatus s);
    const char* enumeration_status_name(EnumerationStatus s);
    const char* spirv_status_name(SpirvReadStatus s);
    const char* frame_stage_name(FrameStage s);
    const char* maintenance_status_name(SwapchainMaintenanceSupportStatus s);
    bool check_instance_extensions(
        const InstanceExtensionEnumeration& e,
        bool& has_surface, bool& has_caps2,
        bool& has_props2, bool& has_surf_maint1);
    bool check_device_extensions(
        const DeviceExtensionEnumeration& e,
        bool& has_swapchain, bool& has_maint1);

    struct WindowFrameFacts {
    bool gpu_in_flight_pending = false;
    bool gpu_fatal_latched     = false;
    bool has_unhandled_acquire = false;
};

struct CameraState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 2.5f;
};

enum class WindowRenderMode { Textured, MvpTextured, NormalDebug };

struct CameraMvpResult {
    bool success = false;
    MvpConfig config{};
    std::string error;
};

std::array<float, 4> make_unit_light(const LightConfig& light) {
    const double x = static_cast<double>(light.direction_to_light_x);
    const double y = static_cast<double>(light.direction_to_light_y);
    const double z = static_cast<double>(light.direction_to_light_z);
    const double len = std::hypot(x, y, z);
    if (!std::isfinite(len) || len <= 0.0) {
        return {0.0f, 0.0f, 1.0f, light.intensity};
    }
    return {
        static_cast<float>(x / len),
        static_cast<float>(y / len),
        static_cast<float>(z / len),
        light.intensity
    };
}

std::array<float, 12> pack_normal_columns(const std::array<float, 16>& n) {
    return {
        n[0], n[1], n[2],  0.0f,
        n[4], n[5], n[6],  0.0f,
        n[8], n[9], n[10], 0.0f
    };
}

std::array<float, 12> pack_normal_view_columns(
    const std::array<float, 16>& n, const std::array<float, 3>& q) {
    auto result = pack_normal_columns(n);
    result[3] = q[0];
    result[7] = q[1];
    result[11] = q[2];
    return result;
}

CameraMvpResult make_camera_mvp(
    VkExtent2D extent, WindowRenderMode mode,
    const CameraState& camera, const SceneConfig& scene_config){
    MvpConfig result{};
    if (mode == WindowRenderMode::Textured) {
        return {true, result, {}};
    }
    std::array<float, 3> q{};
    if (mode == WindowRenderMode::MvpTextured) {
            const double qx = static_cast<double>(camera.x) -
                            static_cast<double>(scene_config.object.translation_x);
            const double qy = static_cast<double>(camera.y) -
                            static_cast<double>(scene_config.object.translation_y);
            const double qz = static_cast<double>(camera.z) -
                            static_cast<double>(scene_config.object.translation_z);
        if (!std::isfinite(qx) || !std::isfinite(qy) || !std::isfinite(qz)) {
            return {false, {}, "camera relative position Q contains non-finite value"};
        }

        const double limit =
            static_cast<double>((std::numeric_limits<float>::max)());
        if (std::abs(qx) > limit || std::abs(qy) > limit || std::abs(qz) > limit) {
            return {false, {}, "camera relative position Q exceeds float max limit"};
        }

        q = {
            static_cast<float>(qx),
            static_cast<float>(qy),
            static_cast<float>(qz),
        };
    }
    result.enabled = true;

    const ProjectionConfig& projection_config = scene_config.projection;
    const float aspect = extent.width / static_cast<float>(extent.height);
    const float fov_y_radians = projection_config.fov_y_radians;
    const float z_near = projection_config.z_near;
    const float z_far = projection_config.z_far;

    auto model = identity_mat4();
    auto normal_to_world = identity_mat4();

    if (mode == WindowRenderMode::NormalDebug) {
        model = multiply_mat4(
            translation_mat4(
                scene_config.object.translation_x,
                scene_config.object.translation_y,
                scene_config.object.translation_z),
            scale_mat4(1.5f, 0.75f, 1.0f));
        normal_to_world = scale_mat4(
            1.0f / 1.5f, 1.0f / 0.75f, 1.0f);
    } else {
        model = translation_mat4(
            scene_config.object.translation_x,
            scene_config.object.translation_y,
            scene_config.object.translation_z);
        normal_to_world = identity_mat4();
    }

    const auto view = translation_mat4(-camera.x, -camera.y, -camera.z);
    const auto projection = perspective_vulkan_mat4(
        fov_y_radians, aspect, z_near, z_far);
    const auto vm = multiply_mat4(view, model);
    result.push.mvp = multiply_mat4(projection, vm);
    result.push.light_direction_intensity = make_unit_light(scene_config.light);
    if (mode == WindowRenderMode::MvpTextured) {
        result.push.normal_columns_camera_relative =
            pack_normal_view_columns(normal_to_world, q);
    } else {
        result.push.normal_columns_camera_relative =
            pack_normal_columns(normal_to_world);
    }
    return {true, result, {}};
}

int run_window_session(WindowSession& window_session,const CpuMesh& mesh,
    WindowRenderMode mode,const SceneConfig& scene_config) {
    WindowFrameFacts facts{};
    ExitConsumeState exit_consume{};

    bool frame_failed           = false;
    bool idle_ok                = false;
    std::uint32_t submit_calls  = 0;
    std::uint32_t present_calls = 0;
    VkResult last_frame_result  = VK_SUCCESS;
    bool last_frame_result_valid = true;
    std::optional<FrameStage> last_stage;
    bool frame_attempted        = false;
    CameraState camera{};
    const bool mvp_enabled = mode != WindowRenderMode::Textured;

    // =================================================================
    // 阶段 1：门控与设备创建
    // =================================================================
    InstanceExtensionEnumeration inst_ext =
        enumerate_instance_extensions(&vkEnumerateInstanceExtensionProperties);
    if (inst_ext.result != VK_SUCCESS && inst_ext.result != VK_INCOMPLETE) {
        close_vulkan_window(window_session);
        return 1;
    }
    bool has_surface = false, has_caps2 = false;
    bool has_props2 = false, has_surf_maint1 = false;
    if (!check_instance_extensions(inst_ext, has_surface, has_caps2,
                                   has_props2, has_surf_maint1)) {
        close_vulkan_window(window_session);
        return 6;
    }

    InstanceConfig inst_cfg{};
    inst_cfg.application_name = "vk01_instance_smoke";
    inst_cfg.api_version      = VK_API_VERSION_1_0;

    WindowExtensionsAttempt extattempt = required_window_instance_extensions(window_session);
    if (!extattempt.ok) {
        close_vulkan_window(window_session);
        print_window_error("window_extensions", extattempt.error);
        return 4;
    }
    auto push_unique = [&](const char* name) {
        for (const auto& s : inst_cfg.extensions) {
            if (std::strcmp(s, name) == 0) return;
        }
        inst_cfg.extensions.push_back(name);
    };
    for (std::uint32_t i = 0; i < extattempt.count; ++i) push_unique(extattempt.names[i]);
    push_unique(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    push_unique(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    push_unique(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

    InstanceAttempt inst = create_instance(inst_cfg, &vkCreateInstance);
    if (inst.result != VK_SUCCESS || inst.instance == VK_NULL_HANDLE) {
        close_vulkan_window(window_session);
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (create_window_surface(inst.instance, window_session.window, &surface) != VK_SUCCESS) {
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 4;
    }

    PFN_vkGetPhysicalDeviceFeatures2KHR get_features2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
            vkGetInstanceProcAddr(inst.instance, "vkGetPhysicalDeviceFeatures2KHR"));
    if (get_features2 == nullptr) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 6;
    }

    DeviceEnumeration dev_enum = enumerate_devices(inst.instance, &vkEnumeratePhysicalDevices);
    if (dev_enum.status != EnumerationStatus::Complete) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 6;
    }
    std::vector<DeviceInfo> snapshots;
    std::vector<std::vector<QueueFamilyInfo>> qfs;
    snapshots.reserve(dev_enum.count);
    qfs.reserve(dev_enum.count);
    for (std::size_t i = 0; i < dev_enum.count && i < dev_enum.devices.size(); ++i) {
        snapshots.push_back(query_device_info(dev_enum.devices[i], &vkGetPhysicalDeviceProperties));
        qfs.push_back(query_queue_families(dev_enum.devices[i],
                                           &vkGetPhysicalDeviceQueueFamilyProperties));
    }
    std::optional<DeviceSelection> sel = select_target_graphics_device(snapshots, qfs);
    if (!sel.has_value()) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 3;
    }
    VkPhysicalDevice phys         = dev_enum.devices[sel->device_index];
    std::uint32_t    family_index = sel->queue_family_index;
    // Codex engineering instrumentation: report the actual selected snapshot.
    const DeviceInfo& selected_info = snapshots[sel->device_index];
    std::cout << "window stage=selected_device"
              << " device_index=" << sel->device_index
              << " family_index=" << family_index
              << " name=" << std::quoted(selected_info.deviceName)
              << " vendor_id=" << selected_info.vendorID
              << " device_type=" << static_cast<int>(selected_info.deviceType)
              << " api_version=" << selected_info.apiVersion << "\n";

    SurfaceSupportAttempt sufsup = query_surface_support(
        phys, family_index, surface, &vkGetPhysicalDeviceSurfaceSupportKHR);
    if (sufsup.result != VK_SUCCESS || sufsup.supported != VK_TRUE) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 4;
    }

    DeviceExtensionEnumeration dev_ext =
        enumerate_device_extensions(phys, &vkEnumerateDeviceExtensionProperties);
    if (dev_ext.status != EnumerationStatus::Complete) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 6;
    }
    bool has_swapchain = false, has_maint1 = false;
    if (!check_device_extensions(dev_ext, has_swapchain, has_maint1)) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 6;
    }

    SwapchainMaintenanceQueryConfig qcfg{};
    qcfg.physical_device               = phys;
    qcfg.instance_dependencies_enabled = true;
    qcfg.device_extensions_supported   = true;
    SwapchainMaintenanceSupportAttempt qatt =
        query_swapchain_maintenance1(qcfg, get_features2);
    std::cout << "window stage=query_maintenance"
              << " status="       << maintenance_status_name(qatt.status)
              << " query_called=" << (qatt.query_called ? 1 : 0)
              << " supported="    << (qatt.supported ? 1 : 0)
              << "\n";
    if (qatt.status != SwapchainMaintenanceSupportStatus::Supported) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 6;
    }

    DeviceConfig dev_cfg{};
    dev_cfg.physical_device               = phys;
    dev_cfg.queue_family_index            = family_index;
    dev_cfg.extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
    };
    dev_cfg.enable_swapchain_maintenance1 = true;
    DeviceQueueAttempt dev_q = create_device_with_queue(
        dev_cfg, &vkCreateDevice, &vkGetDeviceQueue);
    if (dev_q.result != VK_SUCCESS || dev_q.device == VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
        return 3;
    }

    // 阶段 1 之后的可用句柄统一从本地变量借用（SwapchainSession 不再持有它们）
    VkDevice device = dev_q.device;
    VkQueue  queue  = dev_q.queue;

    const auto vertex_bytes =
        mesh.vertices.size() * sizeof(MeshVertex);
    const auto index_bytes =
        mesh.indices.size() * sizeof(std::uint32_t);
    const auto texture_pixels =(mode == WindowRenderMode::MvpTextured)
        ? texture_data::rgba8
        : texture_data::make_tinted_rgba8(scene_config.material);
    const VkDeviceSize image_size = texture_pixels.size();
    BufferAllocation staging_buffer{};
    BufferResourcesApi buf_api{};
    buf_api.create_buffer = &vkCreateBuffer;
    buf_api.get_buffer_memory_requirements = &vkGetBufferMemoryRequirements;
    buf_api.allocate_memory = &vkAllocateMemory;
    buf_api.bind_buffer_memory = &vkBindBufferMemory;
    buf_api.map_memory = &vkMapMemory;
    buf_api.unmap_memory = &vkUnmapMemory;
    buf_api.destroy_buffer = &vkDestroyBuffer;
    buf_api.free_memory = &vkFreeMemory;

    BufferAllocation vertex_buffer{};
    BufferAllocation index_buffer{};
    BufferAllocation index_staging{};
    VertexUploadContext upload_ctx{};
    VertexUploadContext index_upload_ctx{};

    VertexUploadApi upload_api{};
    upload_api.create_pool  = &vkCreateCommandPool;
    upload_api.create_fence = &vkCreateFence;
    upload_api.allocate     = &vkAllocateCommandBuffers;
    upload_api.begin        = &vkBeginCommandBuffer;
    upload_api.cmd_copy     = &vkCmdCopyBuffer;
    upload_api.cmd_barrier  = &vkCmdPipelineBarrier;
    upload_api.end          = &vkEndCommandBuffer;
    upload_api.submit       = &vkQueueSubmit;
    upload_api.wait         = &vkWaitForFences;
    upload_api.destroy_fence = &vkDestroyFence;
    upload_api.destroy_pool  = &vkDestroyCommandPool;

    TextureImageOwner texture_image{};
    ImageAllocation retained_texture{};
    VkImageView texture_view = VK_NULL_HANDLE;

    ImageResourcesApi image_api{};
    image_api.create_image                  = &vkCreateImage;
    image_api.get_image_memory_requirements = &vkGetImageMemoryRequirements;
    image_api.allocate_memory               = &vkAllocateMemory;
    image_api.bind_image_memory             = &vkBindImageMemory;
    image_api.destroy_image                 = &vkDestroyImage;
    image_api.free_memory                   = &vkFreeMemory;

    texture_image.device = device;
    texture_image.image_api = image_api;

    BufferAllocation texture_staging{};
    ImageUploadContext texture_upload_ctx{};
    ImageUploadApi image_upload_api{};

    image_upload_api.create_pool  = &vkCreateCommandPool;
    image_upload_api.create_fence = &vkCreateFence;
    image_upload_api.allocate     = &vkAllocateCommandBuffers;
    image_upload_api.begin        = &vkBeginCommandBuffer;
    image_upload_api.cmd_copy     = &vkCmdCopyBufferToImage;
    image_upload_api.cmd_barrier  = &vkCmdPipelineBarrier;
    image_upload_api.end          = &vkEndCommandBuffer;
    image_upload_api.submit       = &vkQueueSubmit;
    image_upload_api.wait         = &vkWaitForFences;
    image_upload_api.destroy_fence = &vkDestroyFence;
    image_upload_api.destroy_pool  = &vkDestroyCommandPool;

    TextureBindingResources texture_binding{};
    TextureBindingApi texture_binding_api{};
    texture_binding_api.create_sampler = &vkCreateSampler;
    texture_binding_api.create_set_layout = &vkCreateDescriptorSetLayout;
    texture_binding_api.create_pool = &vkCreateDescriptorPool;
    texture_binding_api.allocate_sets = &vkAllocateDescriptorSets;
    texture_binding_api.update_sets = &vkUpdateDescriptorSets;
    texture_binding_api.destroy_sampler = &vkDestroySampler;
    texture_binding_api.destroy_set_layout = &vkDestroyDescriptorSetLayout;
    texture_binding_api.destroy_pool = &vkDestroyDescriptorPool;

    BufferAllocation material_buffer{};
    MaterialBindingResources material_binding{};
    MaterialBindingApi material_binding_api{};
    material_binding_api.create_set_layout = &vkCreateDescriptorSetLayout;
    material_binding_api.create_pool = &vkCreateDescriptorPool;
    material_binding_api.allocate_sets = &vkAllocateDescriptorSets;
    material_binding_api.update_sets = &vkUpdateDescriptorSets;
    material_binding_api.destroy_pool = &vkDestroyDescriptorPool;
    material_binding_api.destroy_set_layout = &vkDestroyDescriptorSetLayout;

    auto teardown_parents = [&](VkDevice d) {
        if (d != VK_NULL_HANDLE) {
            destroy_material_binding(d, material_binding_api, material_binding);
            destroy_buffer_allocation(d, material_buffer, buf_api);
            destroy_image_upload(texture_upload_ctx,image_upload_api);
            destroy_buffer_allocation(d,texture_staging,buf_api);
            destroy_texture_binding(d, texture_binding, texture_binding_api);
            if (texture_view != VK_NULL_HANDLE) {
                vkDestroyImageView(d, texture_view, nullptr);
                texture_view = VK_NULL_HANDLE;
            }
            texture_image.reset();
            destroy_vertex_upload(index_upload_ctx, upload_api);
            destroy_buffer_allocation(d, index_buffer, buf_api);
            destroy_buffer_allocation(d, index_staging, buf_api);
            destroy_vertex_upload(upload_ctx, upload_api);
            destroy_buffer_allocation(d, vertex_buffer, buf_api);
            destroy_buffer_allocation(d, staging_buffer, buf_api);  
            vkDestroyDevice(d, nullptr);
        }
        vkDestroySurfaceKHR(inst.instance, surface, nullptr);
        vkDestroyInstance(inst.instance, nullptr);
        close_vulkan_window(window_session);
    };

    BufferAllocationConfig buf_cfg{};
    buf_cfg.size = 	vertex_bytes;
    vkGetPhysicalDeviceMemoryProperties(phys, &buf_cfg.mem_props);
    BufferAllocationAttempt attempt = create_buffer_allocation(device, buf_cfg, staging_buffer, buf_api);
    std::cout << "window stage=staging_create"
              << " stage_id=" << static_cast<int>(attempt.stage)
              << " complete=" << (attempt.complete ? 1 : 0)
              << " api_result_valid=" << (attempt.api_result_valid ? 1 : 0);
    if (attempt.api_result_valid) {
        std::cout << " result=" << attempt.result;
    }
    std::cout << " size=" << staging_buffer.size
              << " alloc_size=" << staging_buffer.allocation_size
              << "\n";
    if(!attempt.complete){
        teardown_parents(device); 
        return 7;
    }

    VkResult write_res = write_host_coherent_buffer(
        device, staging_buffer, mesh.vertices.data(), vertex_bytes, buf_api);

    std::cout << "window stage=staging_write"
              << " result=" << write_res
              << " bytes=" << vertex_bytes
              << "\n";
    if(write_res != VK_SUCCESS){
        teardown_parents(device); 
        return 7;
    }

    BufferAllocationConfig vertex_cfg{};
    vertex_cfg.size = vertex_bytes;
    vertex_cfg.mem_props = buf_cfg.mem_props;
    vertex_cfg.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_cfg.required_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    BufferAllocationAttempt vertex_attempt =
        create_buffer_allocation(device, vertex_cfg, vertex_buffer, buf_api);
    std::cout << "window stage=vertex_buffer_create"
              << " stage_id=" << static_cast<int>(vertex_attempt.stage)
              << " complete=" << (vertex_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (vertex_attempt.api_result_valid ? 1 : 0);
    if (vertex_attempt.api_result_valid) {
        std::cout << " result=" << vertex_attempt.result;
    }
    std::cout << " size=" << vertex_buffer.size << "\n";

    if (!vertex_attempt.complete) {
        teardown_parents(device);
        return 7;
    }

    VertexUploadConfig upload_cfg{};
    upload_cfg.device = device;
    upload_cfg.queue  = queue;
    upload_cfg.copy_size = 	vertex_bytes;
    upload_cfg.queue_family_index = family_index;
    upload_cfg.staging_buffer = staging_buffer.buffer;
    upload_cfg.vertex_buffer = vertex_buffer.buffer;

    UploadAttempt prep = prepare_vertex_upload(upload_cfg, upload_api, upload_ctx);

    std::cout << "window stage=vertex_upload_prepare"
              << " state=" << static_cast<int>(prep.state)
              << " stage=" << static_cast<int>(prep.stage)
              << " api_result_valid=" << (prep.api_result_valid ? 1 : 0)
              << " result=" << prep.result
              << " submit_attempted=" << (prep.submit_attempted ? 1 : 0)
              << " submit_success=" << (prep.submit_success ? 1 : 0)
              << "\n";

    if(prep.state != UploadState::Prepared){
        if(prep.submit_attempted || prep.state == UploadState::Blocked){
            texture_image.release_to(retained_texture);
            return 1;
        }
        teardown_parents(device);
        return 7;
    }

    constexpr std::uint64_t kShortWaitNs = 16ull * 1000ull * 1000ull;
    bool close_requested = false;

    UploadAttempt upload{};

    for (;;) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window_session.window)) {
            close_requested = true;
        }

        upload = try_complete_vertex_upload(upload_ctx, upload_api, kShortWaitNs);

        if (upload.state == UploadState::Pending) continue;
        if (upload.state == UploadState::Blocked) break;
        if (upload.state == UploadState::Complete) break;
        break; // 非法/意外状态，保留现场
    }   
    std::cout << "window stage=vertex_upload_result"
              << " state=" << static_cast<int>(upload.state)
              << " stage=" << static_cast<int>(upload.stage)
              << " api_result_valid=" << (upload.api_result_valid ? 1 : 0)
              << " result=" << upload.result
              << " submit_attempted=" << (upload.submit_attempted ? 1 : 0)
              << " submit_success=" << (upload.submit_success ? 1 : 0)
              << "\n";

    if (upload.state != UploadState::Complete) {
        return 1; // 不调用 teardown_parents，保留现场
    }

    destroy_vertex_upload(upload_ctx, upload_api);
    destroy_buffer_allocation(device, staging_buffer, buf_api);

    if (close_requested) {
        destroy_buffer_allocation(device, vertex_buffer, buf_api);
        teardown_parents(device);
        return 0;
    }

    BufferAllocationConfig index_staging_cfg{};
    index_staging_cfg.size      = index_bytes;
    index_staging_cfg.mem_props = buf_cfg.mem_props;

    BufferAllocationAttempt index_staging_attempt =
        create_buffer_allocation(device, index_staging_cfg, index_staging, buf_api);
    std::cout << "window stage=index_staging_create"
              << " stage_id=" << static_cast<int>(index_staging_attempt.stage)
              << " complete=" << (index_staging_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (index_staging_attempt.api_result_valid ? 1 : 0);
    if (index_staging_attempt.api_result_valid) {
        std::cout << " result=" << index_staging_attempt.result;
    }
    std::cout << " size=" << index_staging.size
              << " alloc_size=" << index_staging.allocation_size << "\n";
    if (!index_staging_attempt.complete) {
        teardown_parents(device);
        return 7;
    }

    VkResult index_write_res = write_host_coherent_buffer(
        device, index_staging,
        	mesh.indices.data(), index_bytes, buf_api);
    std::cout << "window stage=index_staging_write"
              << " result=" << static_cast<int>(index_write_res)
              << " bytes=" << index_bytes << "\n";
    if (index_write_res != VK_SUCCESS) {
        teardown_parents(device);
        return 7;
    }

    BufferAllocationConfig index_cfg{};
    index_cfg.size = index_bytes;
    index_cfg.mem_props = buf_cfg.mem_props;
    index_cfg.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    index_cfg.required_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    BufferAllocationAttempt index_alloc_attempt =
        create_buffer_allocation(device, index_cfg, index_buffer, buf_api);
    std::cout << "window stage=index_buffer_create"
              << " stage_id=" << static_cast<int>(index_alloc_attempt.stage)
              << " complete=" << (index_alloc_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (index_alloc_attempt.api_result_valid ? 1 : 0);
    if (index_alloc_attempt.api_result_valid) {
        std::cout << " result=" << index_alloc_attempt.result;
    }
    std::cout << " size=" << index_buffer.size << "\n";
    if (!index_alloc_attempt.complete) {
        teardown_parents(device);
        return 7;
    }

    // 使用独立上传上下文；两路上传 Complete 后才进入后面的代构建。
    VertexUploadConfig index_upload_cfg{};
    index_upload_cfg.device = device;
    index_upload_cfg.queue = queue;
    index_upload_cfg.queue_family_index = family_index;
    index_upload_cfg.staging_buffer = index_staging.buffer;
    index_upload_cfg.vertex_buffer = index_buffer.buffer;
    index_upload_cfg.copy_size = index_bytes;
    index_upload_cfg.consumer_access = VK_ACCESS_INDEX_READ_BIT;

    UploadAttempt index_prep =
        prepare_vertex_upload(index_upload_cfg, upload_api, index_upload_ctx);
    std::cout << "window stage=index_upload_prepare"
              << " state=" << static_cast<int>(index_prep.state)
              << " stage=" << static_cast<int>(index_prep.stage)
              << " api_result_valid=" << (index_prep.api_result_valid ? 1 : 0)
              << " result=" << index_prep.result
              << " submit_attempted=" << (index_prep.submit_attempted ? 1 : 0)
              << " submit_success=" << (index_prep.submit_success ? 1 : 0) << "\n";
    if (index_prep.state != UploadState::Prepared) {
        if (index_prep.submit_attempted || index_prep.state == UploadState::Blocked) {
            texture_image.release_to(retained_texture);
            return 1;
        }
        teardown_parents(device);
        return 7;
    }

    UploadAttempt index_upload{};
    for (;;) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window_session.window)) {
            close_requested = true;
        }
        index_upload = try_complete_vertex_upload(
            index_upload_ctx, upload_api, kShortWaitNs);
        if (index_upload.state == UploadState::Pending) continue;
        break;
    }
    std::cout << "window stage=index_upload_result"
              << " state=" << static_cast<int>(index_upload.state)
              << " stage=" << static_cast<int>(index_upload.stage)
              << " api_result_valid=" << (index_upload.api_result_valid ? 1 : 0)
              << " result=" << index_upload.result
              << " submit_attempted=" << (index_upload.submit_attempted ? 1 : 0)
              << " submit_success=" << (index_upload.submit_success ? 1 : 0) << "\n";
    if (index_upload.state != UploadState::Complete) {
        return 1; // 未确认完成，保留上下文及所有被借用的资源。
    }

    destroy_vertex_upload(index_upload_ctx, upload_api);
    destroy_buffer_allocation(device, index_staging, buf_api);
    if (close_requested) {
        teardown_parents(device);
        return 0;
    }

    ImageAllocationConfig texture_cfg{};
    texture_cfg.extent = {2, 2, 1};
    texture_cfg.format = VK_FORMAT_R8G8B8A8_UNORM;
    texture_cfg.mem_props = buf_cfg.mem_props;
    texture_cfg.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                      | VK_IMAGE_USAGE_SAMPLED_BIT;
    texture_cfg.required_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto texture_attempt = create_image_allocation(
        device, texture_cfg, texture_image.allocation,image_api);
    
    std::cout << "window stage=texture_create"
              << " stage_id=" << static_cast<int>(texture_attempt.stage)
              << " complete=" << (texture_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (texture_attempt.api_result_valid ? 1 : 0);
     if (texture_attempt.api_result_valid) {
        std::cout << " result=" << static_cast<int>(texture_attempt.result);
    }
    std::cout << "\n";
    if (!texture_attempt.complete) {
        teardown_parents(device);
        return 1;
    }

    ImageViewConfig texture_view_cfg{};
    texture_view_cfg.device = device;
    texture_view_cfg.image  = texture_image.allocation.image;
    texture_view_cfg.format = texture_cfg.format;

    auto texture_view_attempt = create_image_view(
        texture_view_cfg, &vkCreateImageView);
    if (texture_view_attempt.result != VK_SUCCESS) {
        std::cout << "window stage=texture_view_create status=Failed"
                  << " result=" << static_cast<int>(texture_view_attempt.result)
                  << "\n";
        teardown_parents(device);
        return 1;
    }
    texture_view = texture_view_attempt.view;

     BufferAllocationConfig texture_staging_cfg{};
    texture_staging_cfg.size = image_size;
    texture_staging_cfg.mem_props = texture_cfg.mem_props;

    BufferAllocationAttempt texture_staging_attempt =
        create_buffer_allocation(device, texture_staging_cfg, texture_staging, buf_api);
    std::cout << "window stage=texture_staging_create"
              << " stage_id=" << static_cast<int>(texture_staging_attempt.stage)
              << " complete=" << (texture_staging_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (texture_staging_attempt.api_result_valid ? 1 : 0);
    if (texture_staging_attempt.api_result_valid) {
        std::cout << " result=" << texture_staging_attempt.result;
    }
    std::cout << " size=" << texture_staging.size
              << " alloc_size=" << texture_staging.allocation_size << "\n";
    if (!texture_staging_attempt.complete) {
        teardown_parents(device);
        return 7;
    }

    VkResult texture_write_res = write_host_coherent_buffer(
        device, texture_staging,
        texture_pixels.data(), image_size, buf_api);
    std::cout << "window stage=texture_staging_write"
              << " result=" << static_cast<int>(texture_write_res)
              << " bytes=" << image_size << "\n";
    if (texture_write_res != VK_SUCCESS) {
        teardown_parents(device);
        return 7;
    }

    ImageUploadConfig texture_upload_cfg{};
    texture_upload_cfg.device = device;
    texture_upload_cfg.queue = queue;
    texture_upload_cfg.queue_family_index = family_index;
    texture_upload_cfg.staging_buffer = texture_staging.buffer;
    texture_upload_cfg.image = texture_image.allocation.image;
    texture_upload_cfg.extent = texture_cfg.extent;

    UploadAttempt texture_prep =
        prepare_image_upload(texture_upload_cfg, image_upload_api, texture_upload_ctx);
    std::cout << "window stage=texture_upload_prepare"
              << " state=" << static_cast<int>(texture_prep.state)
              << " stage=" << static_cast<int>(texture_prep.stage)
              << " api_result_valid=" << (texture_prep.api_result_valid ? 1 : 0)
              << " result=" << texture_prep.result
              << " submit_attempted=" << (texture_prep.submit_attempted ? 1 : 0)
              << " submit_success=" << (texture_prep.submit_success ? 1 : 0) << "\n";
    if (texture_prep.state != UploadState::Prepared) {
        if (texture_prep.submit_attempted || texture_prep.state == UploadState::Blocked) {
            texture_image.release_to(retained_texture);
            return 1;
        }
        teardown_parents(device);
        return 7;
    }

    UploadAttempt texture_upload{};
    for (;;) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window_session.window)) {
            close_requested = true;
        }
        texture_upload = try_complete_image_upload(
            texture_upload_ctx, image_upload_api, kShortWaitNs);
        if (texture_upload.state == UploadState::Pending) continue;
        break;
    }
    std::cout << "window stage=texture_upload_result"
              << " state=" << static_cast<int>(texture_upload.state)
              << " stage=" << static_cast<int>(texture_upload.stage)
              << " api_result_valid=" << (texture_upload.api_result_valid ? 1 : 0)
              << " result=" << texture_upload.result
              << " submit_attempted=" << (texture_upload.submit_attempted ? 1 : 0)
              << " submit_success=" << (texture_upload.submit_success ? 1 : 0) << "\n";
    if (texture_upload.state != UploadState::Complete) {
        texture_image.release_to(retained_texture);
        return 1; // 未确认完成，保留上下文及所有被借用的资源。
    }

    destroy_image_upload(texture_upload_ctx, image_upload_api);
    destroy_buffer_allocation(device, texture_staging, buf_api);
    if (close_requested) {
        teardown_parents(device);
        return 0;
    }

    TextureBindingAttempt binding_attempt = create_texture_binding(
        device, texture_view, texture_binding, texture_binding_api);
    // Codex engineering instrumentation: reports the actual creation attempt.
    std::cout << "window stage=texture_binding_create"
              << " stage_id=" << static_cast<int>(binding_attempt.stage)
              << " complete=" << (binding_attempt.complete ? 1 : 0)
              << " api_result_valid=" << (binding_attempt.api_result_valid ? 1 : 0)
              << " result=" << static_cast<int>(binding_attempt.result) << "\n";
    if (!binding_attempt.complete) {
        teardown_parents(device);
        return 1;
    }

    if (mode == WindowRenderMode::MvpTextured) {
        const auto material_cpu = make_material_uniform(scene_config.material);
        BufferAllocationConfig material_cfg{};
        material_cfg.size            = sizeof(MaterialUniform);
        material_cfg.mem_props       = buf_cfg.mem_props;
        material_cfg.usage           = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        material_cfg.required_flags  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        const auto material_attempt = create_buffer_allocation(
            device, material_cfg, material_buffer, buf_api);
        if(!material_attempt.complete){
            teardown_parents(device);
            return 7;
        }
        const VkResult material_write_result = write_host_coherent_buffer(
            device, material_buffer, &material_cpu,
            sizeof(material_cpu), buf_api);
        if(material_write_result != VK_SUCCESS){
            teardown_parents(device);
            return 7;
        }
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = material_buffer.buffer;
        buffer_info.offset = 0;
        buffer_info.range  = sizeof(MaterialUniform);
        const auto material_binding_attempt = create_material_binding(
            device, buffer_info, material_binding_api, material_binding);
        if(!material_binding_attempt.complete){
            teardown_parents(device);
            return 1;
        }
        std::cout << "material stage=material_binding_create"
                  << " stage_id=" << static_cast<int>(material_binding_attempt.stage)
                  << " complete=" << (material_binding_attempt.complete ? 1 : 0)
                  << " api_result_valid=" << (material_binding_attempt.api_result_valid ? 1 : 0);
        if (material_binding_attempt.api_result_valid) {
            std::cout << " result=" << static_cast<int>(material_binding_attempt.result);
        }
        std::cout << "\n";
    }
    // =================================================================
    // 阶段 2：着色器加载、首代构建与发布
    // =================================================================

    // 2.1 表面参数与选项
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window_session.window, &fbw, &fbh);
    while (fbw == 0 || fbh == 0) {
        glfwWaitEvents();
        if (glfwWindowShouldClose(window_session.window)) {
            teardown_parents(device);
            return 0;
        }
        glfwGetFramebufferSize(window_session.window, &fbw, &fbh);
    }
    VkExtent2D fb_extent{
        static_cast<std::uint32_t>(fbw),
        static_cast<std::uint32_t>(fbh)
    };

    SurfaceCapabilitiesAttempt caps =
        query_surface_capabilities(phys, surface, &vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    if (caps.result != VK_SUCCESS) { teardown_parents(device); return 4; }

    SurfaceFormatsSnapshot fmts =
        query_surface_formats(phys, surface, &vkGetPhysicalDeviceSurfaceFormatsKHR);
    PresentModesSnapshot modes =
        query_surface_present_modes(phys, surface, &vkGetPhysicalDeviceSurfacePresentModesKHR);
    if (!(fmts.result == VK_SUCCESS && fmts.status == SurfaceListStatus::Complete && fmts.count > 0) ||
        !(modes.result == VK_SUCCESS && modes.status == SurfaceListStatus::Complete && modes.count > 0)) {
        teardown_parents(device);
        return 4;
    }

    auto chosen_format = choose_swapchain_surface_format(fmts.formats);
    auto chosen_mode   = choose_swapchain_present_mode(modes.modes);
    auto chosen_alpha  = choose_swapchain_composite_alpha(
        caps.capabilities.supportedCompositeAlpha);
    if (!chosen_format.has_value() || !chosen_mode.has_value() || !chosen_alpha.has_value() ||
        (caps.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        teardown_parents(device);
        return 4;
    }

    std::optional<VkExtent2D> planned_extent =
        choose_swapchain_extent(caps.capabilities, fb_extent);
    if (!planned_extent.has_value()) { 
        teardown_parents(device); 
        return 4; 
    }
    if (mode == WindowRenderMode::MvpTextured && !supports_linear_srgb_output(*chosen_format)) {
        std::cout << "window stage=surface_selection status=UnsupportedOutputFormat\n";
        teardown_parents(device);
        return 4;
    }
    std::uint32_t image_count = choose_swapchain_image_count(caps.capabilities);

    auto result = make_camera_mvp(*planned_extent, mode, camera, scene_config);
    if (!result.success) {
        print_window_error("camera_mvp", result.error);
        teardown_parents(device);
        return 1;
    }

    // 2.2 SwapchainSession：只承担会话级同步原语
    SwapchainSession session{};
    if (create_frame_sync_primitives(
            device,
            &session.image_available,
            &session.in_flight,
            &vkCreateSemaphore, &vkCreateFence, &vkDestroySemaphore) != VK_SUCCESS) {
        // 函数契约：失败时出参已清零，无需二次销毁
        teardown_parents(device);
        return 1;
    }

        // 2.3 加载着色器（build 前完成，失败计入退出码 8）
    const char* vs_spirv_path = mvp_enabled
        ? "shaders/textured_mvp.vert.spv"
        : "shaders/textured.vert.spv";

    const char* fs_spirv_path = "shaders/textured.frag.spv";
    if (mode == WindowRenderMode::NormalDebug) {
        fs_spirv_path = "shaders/normal_debug.frag.spv";
    } else if (mode == WindowRenderMode::MvpTextured) {
        fs_spirv_path = "shaders/textured_lit.frag.spv";
    }

    SpirvReadResult vs_spirv = read_spirv_words(vs_spirv_path);
    if (vs_spirv.status != SpirvReadStatus::Complete) {
        std::cout << "window stage=shader_read shader=vertex status="
                  << spirv_status_name(vs_spirv.status) << "\n";
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 8;
    }
    SpirvReadResult fs_spirv = read_spirv_words(fs_spirv_path);
    if (fs_spirv.status != SpirvReadStatus::Complete) {
        std::cout << "window stage=shader_read shader=fragment status="
                  << spirv_status_name(fs_spirv.status) << "\n";
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 8;
    }
    ShaderModuleAttempt vs =
        create_shader_module(device, vs_spirv.words, &vkCreateShaderModule);
    if (vs.result != VK_SUCCESS || vs.module == VK_NULL_HANDLE) {
        std::cout << "window stage=shader_module shader=vertex result="
                  << static_cast<int>(vs.result) << "\n";
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 8;
    }

    if (fs_spirv.status != SpirvReadStatus::Complete) {
        std::cout << "window stage=shader_read shader=fragment status="
                  << spirv_status_name(fs_spirv.status) << "\n";
        vkDestroyShaderModule(device, vs.module, nullptr);
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 8;
    }
    ShaderModuleAttempt fs =
        create_shader_module(device, fs_spirv.words, &vkCreateShaderModule);
    if (fs.result != VK_SUCCESS || fs.module == VK_NULL_HANDLE) {
        std::cout << "window stage=shader_module shader=fragment result="
                  << static_cast<int>(fs.result) << "\n";
        vkDestroyShaderModule(device, vs.module, nullptr);
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 8;
    }

    // 2.4 候选代构建：按 GenerationBuildConfig 的冻结字段填充
    GenerationBuildConfig build_cfg{};
    build_cfg.swapchain.device          = device;
    build_cfg.swapchain.surface         = surface;
    build_cfg.swapchain.min_image_count = image_count;
    build_cfg.swapchain.surface_format  = *chosen_format;
    build_cfg.swapchain.extent          = *planned_extent;
    build_cfg.swapchain.pre_transform   = caps.capabilities.currentTransform;
    build_cfg.swapchain.composite_alpha = *chosen_alpha;
    build_cfg.swapchain.present_mode    = *chosen_mode;

    build_cfg.graphics_queue_family_index = family_index;
    build_cfg.vertex_module               = vs.module;
    build_cfg.fragment_module             = fs.module;
    build_cfg.vertex_buffer               = vertex_buffer.buffer;
    build_cfg.index_buffer                = index_buffer.buffer;
    build_cfg.texture_set_layout = texture_binding.set_layout;
    build_cfg.texture_set = texture_binding.set;
    build_cfg.material_set_layout = material_binding.set_layout;
    build_cfg.material_set = material_binding.set;

    build_cfg.index_count =
        static_cast<std::uint32_t>(mesh.indices.size());
    build_cfg.index_type = VK_INDEX_TYPE_UINT32;
    build_cfg.vertex_layout = VertexInputLayout::Mesh;
    build_cfg.mvp = result.config;
    build_cfg.depth_enabled = mvp_enabled;
    build_cfg.mem_props = buf_cfg.mem_props;

    GenerationBuildApi build_api{};
    build_api.create_swapchain         = &vkCreateSwapchainKHR;
    build_api.get_images               = &vkGetSwapchainImagesKHR;
    build_api.create_image_view        = &vkCreateImageView;
    build_api.create_render_pass       = &vkCreateRenderPass;
    build_api.create_framebuffer       = &vkCreateFramebuffer;
    build_api.create_pipeline_layout   = &vkCreatePipelineLayout;
    build_api.create_graphics_pipelines = &vkCreateGraphicsPipelines;
    build_api.create_command_pool      = &vkCreateCommandPool;
    build_api.create_semaphore         = &vkCreateSemaphore;
    build_api.image_resources          = image_api;

    CommandRecordingApi& rec = build_api.recording;
    rec.allocate            = &vkAllocateCommandBuffers;
    rec.begin               = &vkBeginCommandBuffer;
    rec.begin_render_pass   = &vkCmdBeginRenderPass;
    rec.bind_pipeline       = &vkCmdBindPipeline;
    rec.draw                = &vkCmdDraw;
    rec.end_render_pass     = &vkCmdEndRenderPass;
    rec.end                 = &vkEndCommandBuffer;
    rec.bind_vertex_buffers = &vkCmdBindVertexBuffers;
    rec.bind_index_buffer   = &vkCmdBindIndexBuffer;
    rec.draw_indexed        = &vkCmdDrawIndexed;
    rec.bind_descriptor_sets = &vkCmdBindDescriptorSets;
    rec.push_constants      = &vkCmdPushConstants;
    // 不再需要 create_shader_module：build 接收已创建的 VkShaderModule

    SwapchainGenerationDestroyApi destroy_api{};
    destroy_api.destroy_swapchain       = &vkDestroySwapchainKHR;
    destroy_api.destroy_image_view      = &vkDestroyImageView;
    destroy_api.destroy_render_pass     = &vkDestroyRenderPass;
    destroy_api.destroy_framebuffer     = &vkDestroyFramebuffer;
    destroy_api.destroy_pipeline        = &vkDestroyPipeline;
    destroy_api.destroy_pipeline_layout = &vkDestroyPipelineLayout;
    destroy_api.destroy_command_pool    = &vkDestroyCommandPool;
    destroy_api.destroy_semaphore       = &vkDestroySemaphore;
    destroy_api.image_resources         = image_api;
    // 不包含 destroy_shader_module：模块所有权留在调用方

    GenerationBuildAttempt build =
        build_candidate_generation(session, build_cfg, build_api, destroy_api);

    // shader 模块在 build 返回后即可销毁（pipeline 已烘焙或本次构建作废），
    // 无论成败都由调用方负责，避免泄漏。
    vkDestroyShaderModule(device, vs.module, nullptr);
    vkDestroyShaderModule(device, fs.module, nullptr);

    std::cout << "window stage=generation_build"
              << " images_status=" << enumeration_status_name(build.images_status)
              << " complete=" << (build.complete && build.images_status == EnumerationStatus::Complete ? 1 : 0)
              << " stage="    << static_cast<int>(build.stage)
              << " result_valid=" << (build.result_valid ? 1 : 0)
              << " result="
              << (build.result_valid
                      ? std::to_string(static_cast<int>(build.result))
                      : "N/A")
              << " depth_attempted=" << (build.depth_allocation_attempted ? 1 : 0)
              << " depth_stage="
              << (build.depth_allocation_attempted
                      ? std::to_string(static_cast<int>(build.depth_allocation.stage))
                      : "N/A")
              << "\n";
    
    if (!build.complete ||
        !build.result_valid ||
        build.result != VK_SUCCESS ||
        build.images_status != EnumerationStatus::Complete) {
        // 半成品已由内部 CandidateGuard 清理，保留并记录真实 stage 与 result
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 5;
    }

// 2.5 Tracker 初始化失败：候选代走 unused 清理
    if (session.pending_slot_ == nullptr) {
        destroy_frame_sync_primitives(device, session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 5;
    }
    VkResult first_tracker_res = swapchain_present_tracker_init(
        session.pending_slot_->tracker, device, &vkCreateFence);
    if (first_tracker_res != VK_SUCCESS) {
        std::cout << "window stage=generation_publish"
                  << " status=PublishFailed"
                  << " reason=TrackerInitFailed"
                  << " result=" << static_cast<int>(first_tracker_res) << "\n";
        destroy_unused_swapchain_generation(device, session.pending_slot_->generation, destroy_api);
        swapchain_present_tracker_destroy(session.pending_slot_->tracker, device, &vkDestroyFence);
        session.pending_slot_.reset();
        destroy_frame_sync_primitives(device, session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 1;
    }
    // 2.6 首代发布失败：候选代走 unused 清理
    if (!publish_first_generation(session) || session.active_slot_ == nullptr) {
        std::cout << "window stage=generation_publish"
                  << " status=PublishFailed"
                  << " active_slot="   << (session.active_slot_  != nullptr ? 1 : 0)
                  << " pending_slot="  << (session.pending_slot_ != nullptr ? 1 : 0)
                  << "\n";

        if (session.pending_slot_ != nullptr) {
            destroy_unused_swapchain_generation(device, session.pending_slot_->generation, destroy_api);
            swapchain_present_tracker_destroy(session.pending_slot_->tracker, device, &vkDestroyFence);
            session.pending_slot_.reset();
        }
        destroy_frame_sync_primitives(device, session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);
        teardown_parents(device);
        return 5;
    }

    std::cout << "window stage=generation_publish"
              << " status=Published"
              << " active_slot=1"
              << " pending_slot=" << (session.pending_slot_ != nullptr ? 1 : 0)
              << "\n";

    // =================================================================
    // 阶段 3：逐帧循环与事实流滚动
    // =================================================================

    // 私有辅助 lambda：每次绘制从当前 active_slot 构造局部借用，不保留全局长寿命引用
    auto make_frame_config = [&](GenerationSlot& active_slot) -> FrameConfig {
        FrameConfig cfg{};
        cfg.device          = device;
        cfg.queue           = queue;
        cfg.swapchain       = active_slot.generation.swapchain;
        cfg.image_available = session.image_available;
        cfg.present_fence   = active_slot.tracker.fence;
        cfg.render_finished = &active_slot.generation.render_finished;
        cfg.in_flight       = session.in_flight;
        cfg.command_buffers = &active_slot.generation.command_buffers;
        return cfg;
    };

    FrameApi fapi{
        &vkWaitForFences,
        &vkAcquireNextImageKHR,
        &vkResetFences,
        &vkQueueSubmit,
        &vkQueuePresentKHR,
    };
    FrameDrawAttempt last_draw{};
    bool             has_last_draw = false;
    const char* frame_loop_stop = "Closed";
    int sampled_fbw = fbw;
    int sampled_fbh = fbh;
    bool need_recreation = false;
    bool active_was_drained_ready = false;
    bool   camera_dirty     = false;   
    bool   reset_move_clock = true;    
    double previous_time    = glfwGetTime(); 
    const float  kCameraSpeed = 1.0f; 
    const double kMaxDeltaSec = 0.05; 

    while (!glfwWindowShouldClose(window_session.window)) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window_session.window)) break;

        // 1. 采样窗口像素尺寸与零尺寸（最小化）等待
        int cur_fbw = 0, cur_fbh = 0;
        glfwGetFramebufferSize(window_session.window, &cur_fbw, &cur_fbh);
        while (cur_fbw == 0 || cur_fbh == 0) {
            reset_move_clock = true; 
            glfwWaitEvents();
            if (glfwWindowShouldClose(window_session.window)) {
                frame_loop_stop = "Closed";
                break;
            }
            glfwGetFramebufferSize(window_session.window, &cur_fbw, &cur_fbh);
        }
        if (glfwWindowShouldClose(window_session.window)) break;

        bool size_changed = (cur_fbw != sampled_fbw || cur_fbh != sampled_fbh);

        // 2. 尺寸变化或交换链次优/过时，触发安全重建
        if (size_changed || need_recreation) {
            need_recreation = false;

            // 2.1 排空当前活跃代
            GenerationDrainStatus drain = GenerationDrainStatus::Retry;
            for (;;) {
                glfwPollEvents();
                if (glfwWindowShouldClose(window_session.window)) {
                    frame_loop_stop = "Closed";
                    break;
                }
                drain = try_drain_active_generation(
                    session, facts.gpu_in_flight_pending, facts.gpu_fatal_latched,
                    facts.has_unhandled_acquire, device, 1000000000ULL,
                    &vkWaitForFences, &vkResetFences);
                if (drain != GenerationDrainStatus::Retry) break;
            }
            if (glfwWindowShouldClose(window_session.window)) {
                frame_loop_stop = "Closed";
                break;
            }
            if (drain != GenerationDrainStatus::Ready) {
                frame_failed = true;
                frame_loop_stop = "DrainFailed";
                break;
            }

            // 2.2 排空后重新采样尺寸（防止排空等待期间尺寸变化）
            glfwGetFramebufferSize(window_session.window, &cur_fbw, &cur_fbh);
            while (cur_fbw == 0 || cur_fbh == 0) {
                reset_move_clock = true;
                glfwWaitEvents();
                if (glfwWindowShouldClose(window_session.window)) {
                    frame_loop_stop = "Closed";
                    break;
                }
                glfwGetFramebufferSize(window_session.window, &cur_fbw, &cur_fbh);
            }
            if (glfwWindowShouldClose(window_session.window)) {
                frame_loop_stop = "Closed";
                break;
            }

            // 2.3 重新查询表面全套能力与选项
            SurfaceCapabilitiesAttempt new_caps = query_surface_capabilities(
                phys, surface, &vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
            SurfaceFormatsSnapshot new_fmts = query_surface_formats(
                phys, surface, &vkGetPhysicalDeviceSurfaceFormatsKHR);
            PresentModesSnapshot new_modes = query_surface_present_modes(
                phys, surface, &vkGetPhysicalDeviceSurfacePresentModesKHR);

            if (new_caps.result != VK_SUCCESS) {
                last_frame_result = new_caps.result;
                last_frame_result_valid = true;
                std::cout << "window stage=swapchain_recreate status=SurfaceQueryFailed"
                          << " query=capabilities result=" << static_cast<int>(new_caps.result) << "\n";
                frame_failed = true;
                frame_loop_stop = "SurfaceCapabilitiesQueryFailed";
                break;
            }
            if (new_fmts.result != VK_SUCCESS) {
                last_frame_result = new_fmts.result;
                last_frame_result_valid = true;
                std::cout << "window stage=swapchain_recreate status=SurfaceQueryFailed"
                          << " query=formats result=" << static_cast<int>(new_fmts.result) << "\n";
                frame_failed = true;
                frame_loop_stop = "SurfaceFormatsQueryFailed";
                break;
            }
            if (new_modes.result != VK_SUCCESS) {
                last_frame_result = new_modes.result;
                last_frame_result_valid = true;
                std::cout << "window stage=swapchain_recreate status=SurfaceQueryFailed"
                          << " query=present_modes result=" << static_cast<int>(new_modes.result) << "\n";
                frame_failed = true;
                frame_loop_stop = "PresentModesQueryFailed";
                break;
            }
            if (new_fmts.status != SurfaceListStatus::Complete || new_fmts.count == 0 ||
                new_modes.status != SurfaceListStatus::Complete || new_modes.count == 0) {
                std::cout << "window stage=swapchain_recreate status=SurfaceListIncomplete"
                          << " fmts_status=" << surface_list_status_name(new_fmts.status)
                          << " fmts_count="  << new_fmts.count
                          << " modes_status=" << surface_list_status_name(new_modes.status)
                          << " modes_count="  << new_modes.count
                          << "\n";
                frame_failed = true;
                frame_loop_stop = "SurfaceListIncomplete";
                break;
            }

            auto chosen_new_format = choose_swapchain_surface_format(new_fmts.formats);
            auto chosen_new_mode   = choose_swapchain_present_mode(new_modes.modes);
            auto chosen_new_alpha  = choose_swapchain_composite_alpha(
                new_caps.capabilities.supportedCompositeAlpha);

            if (!chosen_new_format.has_value() || !chosen_new_mode.has_value() || !chosen_new_alpha.has_value() ||
                (new_caps.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
                std::cout << "window stage=swapchain_recreate status=SurfaceSelectionFailed\n";
                frame_failed = true;
                frame_loop_stop = "SurfaceSelectionFailed";
                break;
            }
            if (mode == WindowRenderMode::MvpTextured && !supports_linear_srgb_output(*chosen_new_format)) {
                std::cout << "window stage=swapchain_recreate status=UnsupportedOutputFormat\n";
                frame_failed = true;
                frame_loop_stop = "UnsupportedOutputFormat";
                last_frame_result_valid = false;
                last_stage.reset();
                break;
            }

            VkExtent2D rebuild_fb_extent{ static_cast<uint32_t>(cur_fbw), static_cast<uint32_t>(cur_fbh) };
            auto new_extent = choose_swapchain_extent(new_caps.capabilities, rebuild_fb_extent);
            if (!new_extent.has_value()) {
                std::cout << "window stage=swapchain_recreate status=ExtentChooseFailed\n";
                frame_failed = true;
                frame_loop_stop = "ExtentChooseFailed";
                break;
            }
            uint32_t new_img_count = choose_swapchain_image_count(new_caps.capabilities);

            auto rebuild_mvp = make_camera_mvp(*new_extent, mode, camera, scene_config);
            if (!rebuild_mvp.success) {
                print_window_error("camera_mvp", rebuild_mvp.error);
                frame_failed = true;
                frame_loop_stop = "CameraMvpFailed";
                last_frame_result_valid = false;
                last_stage.reset();
                break;
            }

            // 更新基准采样尺寸，避免与能力钳制后的 extent 比较导致死循环重建
            sampled_fbw = cur_fbw;
            sampled_fbh = cur_fbh;

            std::cout << "window stage=swapchain_recreate"
                      << " trigger="     << (size_changed ? "SizeChanged" : "SwapchainChanged")
                      << " width="       << new_extent->width
                      << " height="      << new_extent->height
                      << " image_count=" << new_img_count
                      << "\n";

            // 2.4 重新创建本轮临时 Shader 模块
            ShaderModuleAttempt new_vs = create_shader_module(device, vs_spirv.words, &vkCreateShaderModule);
            ShaderModuleAttempt new_fs = create_shader_module(device, fs_spirv.words, &vkCreateShaderModule);
            if (new_vs.result != VK_SUCCESS || new_fs.result != VK_SUCCESS ||
                new_vs.module == VK_NULL_HANDLE || new_fs.module == VK_NULL_HANDLE) {
                if (new_vs.result != VK_SUCCESS) {
                    last_frame_result = new_vs.result;
                    last_frame_result_valid = true;
                } else if (new_fs.result != VK_SUCCESS) {
                    last_frame_result = new_fs.result;
                    last_frame_result_valid = true;
                } else {
                    last_frame_result = VK_ERROR_INITIALIZATION_FAILED;
                    last_frame_result_valid = true;
                }
                std::cout << "window stage=shader_create status=Failed"
                          << " vs_result=" << static_cast<int>(new_vs.result)
                          << " fs_result=" << static_cast<int>(new_fs.result)
                          << "\n";
                if (new_vs.module != VK_NULL_HANDLE) vkDestroyShaderModule(device, new_vs.module, nullptr);
                if (new_fs.module != VK_NULL_HANDLE) vkDestroyShaderModule(device, new_fs.module, nullptr);
                frame_failed = true;
                frame_loop_stop = "ShaderCreateFailed";
                break;
            }

            // 2.5 组装配置，刷新全部表面选项，借用旧链句柄
            GenerationBuildConfig rebuild_cfg = build_cfg;
            rebuild_cfg.swapchain.surface_format  = *chosen_new_format;
            rebuild_cfg.swapchain.present_mode    = *chosen_new_mode;
            rebuild_cfg.swapchain.composite_alpha = *chosen_new_alpha;
            rebuild_cfg.swapchain.pre_transform   = new_caps.capabilities.currentTransform;
            rebuild_cfg.swapchain.extent          = *new_extent;
            rebuild_cfg.swapchain.min_image_count = new_img_count;
            rebuild_cfg.swapchain.old_swapchain   = session.active_slot_->generation.swapchain;
            rebuild_cfg.vertex_module             = new_vs.module;
            rebuild_cfg.fragment_module           = new_fs.module;
            rebuild_cfg.mvp = rebuild_mvp.config;

            // 2.6 构建新候选代
            GenerationBuildAttempt rebuild_attempt = build_candidate_generation(
                session, rebuild_cfg, build_api, destroy_api);

            // 临时 Shader 模块随构建结束立即释放
            vkDestroyShaderModule(device, new_vs.module, nullptr);
            vkDestroyShaderModule(device, new_fs.module, nullptr);

            std::cout << "window stage=generation_build"
                      << " images_status=" << enumeration_status_name(rebuild_attempt.images_status)
                      << " complete=" << (rebuild_attempt.complete && rebuild_attempt.images_status == EnumerationStatus::Complete ? 1 : 0)
                      << " stage="    << static_cast<int>(rebuild_attempt.stage)
                      << " result_valid=" << (rebuild_attempt.result_valid ? 1 : 0)
                      << " result="
                      << (rebuild_attempt.result_valid
                              ? std::to_string(static_cast<int>(rebuild_attempt.result))
                              : "N/A")
                      << " depth_attempted=" << (rebuild_attempt.depth_allocation_attempted ? 1 : 0)
                      << " depth_stage="
                      << (rebuild_attempt.depth_allocation_attempted
                              ? std::to_string(static_cast<int>(rebuild_attempt.depth_allocation.stage))
                              : "N/A")
                      << " old_retired="  << (rebuild_attempt.old_retired ? 1 : 0)
                      << "\n";

            if (!rebuild_attempt.complete ||
                !rebuild_attempt.result_valid ||
                rebuild_attempt.result != VK_SUCCESS ||
                rebuild_attempt.images_status != EnumerationStatus::Complete) {
                last_frame_result_valid = rebuild_attempt.result_valid;
                if (last_frame_result_valid) {
                    last_frame_result = rebuild_attempt.result;
                }
                destroy_drained_active_generation(session, device, destroy_api, &vkDestroyFence);
                active_was_drained_ready = true;
                frame_failed = true;
                frame_loop_stop = "BuildFailed";
                break;
            }

            // 2.7 初始化新候选 Tracker
            VkResult tracker_init_res = VK_SUCCESS;
            if (session.pending_slot_ == nullptr) {
                tracker_init_res = VK_ERROR_INITIALIZATION_FAILED;
            } else {
                tracker_init_res = swapchain_present_tracker_init(
                    session.pending_slot_->tracker, device, &vkCreateFence);
            }

            if (tracker_init_res != VK_SUCCESS) {
                std::cout << "window stage=generation_publish"
                          << " status=PublishFailed"
                          << " reason=TrackerInitFailed"
                          << " result=" << static_cast<int>(tracker_init_res)
                          << "\n";
                last_frame_result = tracker_init_res;
                last_frame_result_valid = true;
                if (session.pending_slot_ != nullptr) {
                    destroy_unused_swapchain_generation(device, session.pending_slot_->generation, destroy_api);
                    swapchain_present_tracker_destroy(session.pending_slot_->tracker, device, &vkDestroyFence);
                    session.pending_slot_.reset();
                }
                destroy_drained_active_generation(session, device, destroy_api, &vkDestroyFence);
                active_was_drained_ready = true;
                frame_failed = true;
                frame_loop_stop = "TrackerInitFailed";
                break;
            }

            // 2.8 销毁旧代与发布新代
            destroy_drained_active_generation(session, device, destroy_api, &vkDestroyFence);
            if (!publish_first_generation(session) || session.active_slot_ == nullptr) {
                std::cout << "window stage=generation_publish"
                          << " status=PublishFailed"
                          << " reason=PublishMoveFailed\n";
                if (session.pending_slot_ != nullptr) {
                    destroy_unused_swapchain_generation(device, session.pending_slot_->generation, destroy_api);
                    swapchain_present_tracker_destroy(session.pending_slot_->tracker, device, &vkDestroyFence);
                    session.pending_slot_.reset();
                }
                active_was_drained_ready = true;
                last_frame_result = VK_ERROR_INITIALIZATION_FAILED;
                last_frame_result_valid = true;
                frame_failed = true;
                frame_loop_stop = "PublishFailed";
                break;
            }

            std::cout << "window stage=generation_publish"
                      << " status=Published"
                      << " active_slot=1"
                      << " pending_slot=" << (session.pending_slot_ != nullptr ? 1 : 0)
                      << "\n";

            // 2.9 清除不再适用的旧帧末状态
            has_last_draw = false;
            last_draw = FrameDrawAttempt{};
            last_stage.reset();

            reset_move_clock = true;
            camera_dirty     = false;
            continue;
        }

        if (session.active_slot_ == nullptr) {
            frame_failed = true;
            frame_loop_stop = "Failed";
            break;
        }
        GenerationSlot& current_slot = *session.active_slot_;
        SwapchainPresentTracker& current_tracker = current_slot.tracker;

        if (mvp_enabled) {
            const bool a_pressed =
                (glfwGetKey(window_session.window, GLFW_KEY_A) == GLFW_PRESS);
            const bool d_pressed =
                (glfwGetKey(window_session.window, GLFW_KEY_D) == GLFW_PRESS);

            const double now = glfwGetTime();
            const double elapsed =
                reset_move_clock ? 0.0 : (now - previous_time);
            
            if (!std::isfinite(now) ||
                !std::isfinite(elapsed) ||
                elapsed < 0.0) {
                frame_failed = true;
                frame_loop_stop = "CameraClockInvalid";
                last_frame_result_valid = false;
                last_stage.reset();
                std::cout << "window stage=camera_input status=InvalidTime\n";
                break;
            }

            previous_time    = now;
            reset_move_clock = false;

            double clamped = elapsed;
            if (clamped > kMaxDeltaSec) {
                clamped = kMaxDeltaSec;
            }

            float delta_x = 0.0f;
            if (a_pressed && !d_pressed) {
                delta_x = -static_cast<float>(clamped) * kCameraSpeed;
            } else if (d_pressed && !a_pressed) {
                delta_x =  static_cast<float>(clamped) * kCameraSpeed;
            }
            camera.x += delta_x;
            // 有位移才置 true；无位移保留旧值（上一轮未消费的变化不能被清零）
            camera_dirty = camera_dirty || (delta_x != 0.0f);
        }

        PresentWaitOutcome pw = swapchain_present_tracker_wait_and_rearm(
            current_tracker, device, 1000000000ULL,
            &vkWaitForFences, &vkResetFences);
        if (pw == PresentWaitOutcome::Retry) continue;
        if (pw == PresentWaitOutcome::Failed) {
            facts.gpu_fatal_latched = true;
            frame_loop_stop = "Failed";
            break;
        }

        if (mvp_enabled && camera_dirty) {
            const VkExtent2D active_extent = current_slot.generation.extent;

            auto camera_mvp = make_camera_mvp(active_extent, mode, camera, scene_config);
            if (!camera_mvp.success) {
                print_window_error("camera_mvp", camera_mvp.error);
                frame_failed = true;
                frame_loop_stop = "CameraMvpFailed";
                last_frame_result_valid = false;
                last_stage.reset();
                break;
            }

            // 以首次构建配置为模板：只覆盖活跃代尺寸、MVP 与深度开关
            GenerationBuildConfig mvp_cfg = build_cfg;
            mvp_cfg.swapchain.extent = active_extent;
            mvp_cfg.mvp             = camera_mvp.config;
            mvp_cfg.depth_enabled    = mvp_enabled;

            MvpUpdateAttempt update = update_active_generation_mvp(
                session,
                mvp_cfg,
                mvp_cfg.mvp,               // 同一份 MVP 作为 new_mvp 传入
                build_api,
                destroy_api,
                &vkWaitForFences);

            // 先回填两个 facts（两个独立 if；等待成功与致命可同时成立）
            if (update.in_flight_waited_ok) {
                facts.gpu_in_flight_pending = false;
            }
            if (update.gpu_fatal) {
                facts.gpu_fatal_latched = update.gpu_fatal;
            }

            // 再同步报告有效位；last_stage 记录的是绘制阶段，先清空
            last_frame_result_valid = update.result_valid;
            if (update.result_valid) {
                last_frame_result = update.result;
            }
            last_stage.reset();

            if (update.status == MvpUpdateStatus::Ready) {
                camera_dirty = false;          // 已消费，继续本轮绘制
            } else if (update.status == MvpUpdateStatus::Retry) {
                // 保留 dirty，本轮不绘制，下一轮重新尝试
                continue;
            } else {
                std::cout << "window stage=mvp_update status=Failed"
                          << " stage="  << static_cast<int>(update.stage)
                          << " result_valid=" << (update.result_valid ? 1 : 0)
                          << " result="
                          << (update.result_valid
                                  ? std::to_string(static_cast<int>(update.result))
                                  : "N/A")
                          << " allocated_count=" << update.allocated_count
                          << " recorded_count="  << update.recorded_count
                          << "\n";
                frame_failed    = true;
                frame_loop_stop = "MvpUpdateFailed";
                break;
            }
        }
        
        frame_attempted = true;
        FrameConfig fcfg = make_frame_config(current_slot);
        FrameDrawAttempt d = draw_triangle_frame(fcfg, fapi);
        last_frame_result = d.result;
        last_frame_result_valid = true;
        last_stage        = d.stage;
        if (d.submitted)      ++submit_calls;
        if (d.present_called) ++present_calls;

        // 1. 先按本帧开头 GPU wait SUCCESS 清除旧 GPU pending
        if (d.in_flight_waited_ok) {
            facts.gpu_in_flight_pending = false;
        }
        // 2. 再按本帧末尾 GPU submit SUCCESS 树立新 GPU pending
        if (d.submit_success) {
            facts.gpu_in_flight_pending = true;
        }
        if (d.present_called) {
            swapchain_present_tracker_on_presented(
                current_tracker, d.present_result, /*did_present=*/true);
        }
        if (d.gpu_fatal) {
            facts.gpu_fatal_latched = true;
        }

        // 只要取了图像且提交没成功（Reset 失败 / 两 OOM / 其它非 OOM 硬失败），
        // image_available 就处于已发信号未被消费的状态，必须锁存。
        if (d.image_index != UINT32_MAX && !d.submit_success) {
            facts.has_unhandled_acquire = true;
        }

        last_draw     = d;
        has_last_draw = true;

        if (d.status == FrameResult::Presented || d.status == FrameResult::Retry) continue;
        if (d.status == FrameResult::SwapchainChanged) {
            need_recreation = true;
            continue;
        }
        frame_failed    = true;
        frame_loop_stop = "Failed";
        break;
    }

    std::cout << "window stage=frame_loop"
              << " stop="          << frame_loop_stop
              << " submit_calls="  << submit_calls
              << " present_calls=" << present_calls
              << " last_stage="
              << ((frame_attempted && last_stage.has_value())
                      ? frame_stage_name(*last_stage) : "None")
              << " result="
              << (last_frame_result_valid
                      ? std::to_string(static_cast<int>(last_frame_result))
                      : "N/A")
              << "\n";

    // =================================================================
    // 阶段 4：退出消费、安全排空与层级销毁
    // =================================================================

    const bool can_attempt_exit_consume =
        has_last_draw &&
        facts.has_unhandled_acquire &&
        !facts.gpu_fatal_latched &&
        !facts.gpu_in_flight_pending &&
        (session.active_slot_  != nullptr) &&
        (session.active_slot_->tracker.status == PresentTrackingState::Idle) &&
        (session.pending_slot_ == nullptr) &&
        (last_draw.stage == FrameStage::Submit) &&
        (last_draw.image_index != UINT32_MAX) &&
        last_draw.submitted &&
        !last_draw.submit_success &&
        !last_draw.present_called &&
        (last_draw.result == VK_ERROR_OUT_OF_HOST_MEMORY ||
         last_draw.result == VK_ERROR_OUT_OF_DEVICE_MEMORY);

    ExitConsumeStatus ec = ExitConsumeStatus::Retry;
    if (can_attempt_exit_consume) {
        for (;;) {
            VkResult vr = VK_SUCCESS;
            ec = try_consume_acquire_for_exit(
                device, queue,
                session.image_available, session.in_flight,
                facts.gpu_in_flight_pending,
                facts.gpu_fatal_latched,
                &vkQueueSubmit, &vkWaitForFences,
                1000000000ULL, &exit_consume, &vr);
            if (ec == ExitConsumeStatus::Done) {
                facts.has_unhandled_acquire = false;
                break;
            }
            if (ec == ExitConsumeStatus::Error) break;
            // Retry 继续（循环内不再重新判定 can_attempt_exit_consume）
        }
        std::cout << "window stage=exit_consume"
                  << " status="         << (ec == ExitConsumeStatus::Done ? "Done" : "Error")
                  << " stage="          << static_cast<int>(exit_consume.stage)
                  << " failed_stage="   << static_cast<int>(exit_consume.failed_stage)
                  << " last_vk_result=" << static_cast<int>(exit_consume.last_vk_result)
                  << " original_result="
                  << (last_frame_result_valid
                          ? std::to_string(static_cast<int>(last_frame_result))
                          : "N/A")
                  << "\n";
    } else {
        std::cout << "window stage=exit_consume"
                  << " status=Skipped"
                  << " original_result="
                  << (last_frame_result_valid
                          ? std::to_string(static_cast<int>(last_frame_result))
                          : "N/A")
                  << "\n";
    }

    GenerationDrainStatus drain = GenerationDrainStatus::Retry;
    bool can_clean_parents = false;

    if (session.active_slot_ == nullptr) {
        // 重建失败分支中已凭实际 Ready 销毁了旧代且清理了未投入使用的候选代
        can_clean_parents = active_was_drained_ready && (session.pending_slot_ == nullptr);
        std::cout << "window stage=generation_drain"
                  << " status="             << (can_clean_parents ? "Ready" : "Failed")
                  << " gpu_pending="        << (facts.gpu_in_flight_pending ? 1 : 0)
                  << " gpu_fatal="          << (facts.gpu_fatal_latched ? 1 : 0)
                  << " unhandled_acquire="  << (facts.has_unhandled_acquire ? 1 : 0)
                  << " original_result="
                  << (last_frame_result_valid
                          ? std::to_string(static_cast<int>(last_frame_result))
                          : "N/A")
                  << "\n";
    } else {
        for (;;) {
            drain = try_drain_active_generation(
                session,
                facts.gpu_in_flight_pending,
                facts.gpu_fatal_latched,
                facts.has_unhandled_acquire,
                device, 1000000000ULL,
                &vkWaitForFences, &vkResetFences);
            if (drain != GenerationDrainStatus::Retry) break;
        }

        const char* drain_status_str = "Unknown";
        switch (drain) {
            case GenerationDrainStatus::Ready:   drain_status_str = "Ready";   break;
            case GenerationDrainStatus::Blocked: drain_status_str = "Blocked"; break;
            case GenerationDrainStatus::Failed:  drain_status_str = "Failed";  break;
            case GenerationDrainStatus::Retry:   drain_status_str = "Retry";   break;
        }
        std::cout << "window stage=generation_drain"
                  << " status="             << drain_status_str
                  << " gpu_pending="        << (facts.gpu_in_flight_pending ? 1 : 0)
                  << " gpu_fatal="          << (facts.gpu_fatal_latched ? 1 : 0)
                  << " unhandled_acquire="  << (facts.has_unhandled_acquire ? 1 : 0)
                  << " original_result="
                  << (last_frame_result_valid
                          ? std::to_string(static_cast<int>(last_frame_result))
                          : "N/A")
                  << "\n";

        if (drain == GenerationDrainStatus::Ready) {
            destroy_drained_active_generation(session, device, destroy_api, &vkDestroyFence);
            can_clean_parents = true;
        }
    }

    if (can_clean_parents) {
        destroy_frame_sync_primitives(device,
                                      session.image_available, session.in_flight,
                                      &vkDestroySemaphore, &vkDestroyFence);

        // Codex engineering instrumentation: report only after real cleanup calls.
        std::cout << "window stage=generation_cleanup"
                  << " active_slot=" << (session.active_slot_ != nullptr ? 1 : 0)
                  << " pending_slot=" << (session.pending_slot_ != nullptr ? 1 : 0)
                  << " image_available=" << (session.image_available != VK_NULL_HANDLE ? 1 : 0)
                  << " in_flight=" << (session.in_flight != VK_NULL_HANDLE ? 1 : 0) << "\n";
        VkResult idle = vkDeviceWaitIdle(device);
        idle_ok = (idle == VK_SUCCESS);
        teardown_parents(device);
        std::cout << "window stage=parent_cleanup completed=1 idle_result="
                  << static_cast<int>(idle) << "\n";

        if (frame_failed) return 11;
        if (facts.gpu_fatal_latched) return 11;
        if (!idle_ok) return 12;
        return 0;
    }

    // 非 Ready（Blocked / Failed）：严禁调用普通销毁（包括代内与父级），完好保留现场
    // 不调用 destroy_drained，不调用 destroy_frame_sync，严禁调用 teardown_parents！
    texture_image.release_to(retained_texture);
    if (frame_failed) return 11;
    if (facts.gpu_fatal_latched) return 11;
    return 1;
    }
    // 日志辅助：打印窗口阶段/错误，以及把各类状态枚举转成字符串。
    void print_window_error(const char* stage, const std::string& error) {
        std::cout << "window stage=" << stage
                  << " error=" << std::quoted(error) << "\n";
    }
    const char* surface_list_status_name(SurfaceListStatus s) {
        switch (s) {
            case SurfaceListStatus::Complete:    return "Complete";
            case SurfaceListStatus::Empty:       return "Empty";
            case SurfaceListStatus::CountFailed: return "CountFailed";
            case SurfaceListStatus::FillFailed:  return "FillFailed";
            case SurfaceListStatus::Incomplete:  return "Incomplete";
        }
        return "Unknown";
    }
    const char* enumeration_status_name(EnumerationStatus s) {
    switch (s) {
        case EnumerationStatus::Complete:    return "Complete";
        case EnumerationStatus::Empty:       return "Empty";
        case EnumerationStatus::CountFailed: return "CountFailed";
        case EnumerationStatus::FillFailed:  return "FillFailed";
        case EnumerationStatus::Incomplete:  return "Incomplete";
    }
    return "Unknown";
    }
    const char* spirv_status_name(SpirvReadStatus s) {
        switch (s) {
            case SpirvReadStatus::Complete:    return "Complete";
            case SpirvReadStatus::OpenFailed:  return "OpenFailed";
            case SpirvReadStatus::InvalidSize: return "InvalidSize";
            case SpirvReadStatus::ReadFailed:  return "ReadFailed";
        }
        return "Unknown";
    }
    const char* frame_stage_name(FrameStage s) {
        switch (s) {
            case FrameStage::Wait:    return "Wait";
            case FrameStage::Acquire: return "Acquire";
            case FrameStage::Reset:   return "Reset";
            case FrameStage::Submit:  return "Submit";
            case FrameStage::Present: return "Present";
        }
        return "Unknown";
    }
        const char* maintenance_status_name(SwapchainMaintenanceSupportStatus s) {
        switch (s) {
            case SwapchainMaintenanceSupportStatus::InstanceDependenciesMissing:
                return "InstanceDependenciesMissing";
            case SwapchainMaintenanceSupportStatus::DeviceExtensionsMissing:
                return "DeviceExtensionsMissing";
            case SwapchainMaintenanceSupportStatus::QueryUnavailable:
                return "QueryUnavailable";
            case SwapchainMaintenanceSupportStatus::Unsupported:
                return "Unsupported";
            case SwapchainMaintenanceSupportStatus::Supported:
                return "Supported";
        }
        return "Unknown";
    }

    // 检查实例是否提供 maintenance1 依赖的四个实例扩展。
    bool check_instance_extensions(
        const InstanceExtensionEnumeration& e,
        bool& has_surface, bool& has_caps2,
        bool& has_props2, bool& has_surf_maint1)
    {
        has_surface = has_caps2 = has_props2 = has_surf_maint1 = false;
        if (e.status != EnumerationStatus::Complete) {
            return false;
        }
        for (const auto& ext : e.extensions) {
            if (std::strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                has_surface = true;
            } else if (std::strcmp(ext.extensionName,
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) == 0) {
                has_caps2 = true;
            } else if (std::strcmp(ext.extensionName,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                has_props2 = true;
            } else if (std::strcmp(ext.extensionName,
                VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == 0) {
                has_surf_maint1 = true;
            }
        }
        return has_surface && has_caps2 && has_props2 && has_surf_maint1;
    }

    // 检查设备是否同时提供 swapchain 与 swapchain_maintenance1 扩展。
    bool check_device_extensions(
        const DeviceExtensionEnumeration& e,
        bool& has_swapchain, bool& has_maint1)
    {
        has_swapchain = has_maint1 = false;
        if (e.status != EnumerationStatus::Complete) {
            return false;
        }
        for (const auto& ext : e.extensions) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                has_swapchain = true;
            } else if (std::strcmp(ext.extensionName,
                VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) == 0) {
                has_maint1 = true;
            }
        }
        return has_swapchain && has_maint1;
    }

    // --maintenance-check：实例扩展 → 建实例 → 选设备 → 查设备扩展与特性 → 启用特性建设备，
    // 逐步验证 swapchain_maintenance1 真实可用；不进入渲染流程。
    int run_maintenance_check() {
        // ---------------- 阶段 1：实例扩展 ----------------
        InstanceExtensionEnumeration inst_ext =
            enumerate_instance_extensions(&vkEnumerateInstanceExtensionProperties);

        // 真实 Vulkan 错误（非 SUCCESS 且非 INCOMPLETE）→ 退出码 1
        if (inst_ext.result != VK_SUCCESS && inst_ext.result != VK_INCOMPLETE) {
            std::cout << "maintenance stage=instance_extensions status="
                      << enumeration_status_name(inst_ext.status)
                      << " result=" << static_cast<int>(inst_ext.result)
                      << " query_called=0 supported=0 enabled=0\n";
            return 1;
        }

        bool has_surface = false, has_caps2 = false;
        bool has_props2 = false, has_surf_maint1 = false;
        bool inst_ok = check_instance_extensions(
            inst_ext, has_surface, has_caps2, has_props2, has_surf_maint1);

        if (!inst_ok) {
            std::cout << "maintenance stage=instance_extensions status="
                      << enumeration_status_name(inst_ext.status)
                      << " result=" << static_cast<int>(inst_ext.result)
                      << " count="  << inst_ext.count
                      << " surface="    << has_surface
                      << " caps2="      << has_caps2
                      << " props2="     << has_props2
                      << " surf_maint1=" << has_surf_maint1
                      << " query_called=0 supported=0 enabled=0\n";
            return 6;
        }

        // ---------------- 阶段 2：创建实例 ----------------
        InstanceConfig inst_cfg{};
        inst_cfg.application_name = "vk01_maintenance_check";
        inst_cfg.api_version      = VK_API_VERSION_1_0;
        inst_cfg.extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        };
        InstanceAttempt inst_att = create_instance(inst_cfg, &vkCreateInstance);
        if (inst_att.result != VK_SUCCESS || inst_att.instance == VK_NULL_HANDLE) {
            std::cout << "maintenance stage=create_instance result="
                      << static_cast<int>(inst_att.result) << "\n";
            return 1;
        }
        VkInstance instance = inst_att.instance;

        // ---------------- 阶段 3：加载 KHR 入口 ----------------
        PFN_vkGetPhysicalDeviceFeatures2KHR get_features2 =
            reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
                vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
        if (get_features2 == nullptr) {
            std::cout << "maintenance stage=load_features2 available=0"
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 6;
        }

        // ---------------- 阶段 4：设备枚举与设备扩展 ----------------
        DeviceEnumeration dev_enum =
            enumerate_devices(instance, &vkEnumeratePhysicalDevices);

        if (dev_enum.result != VK_SUCCESS && dev_enum.result != VK_INCOMPLETE) {
            std::cout << "maintenance stage=enumerate_devices status="
                      << enumeration_status_name(dev_enum.status)
                      << " result=" << static_cast<int>(dev_enum.result)
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 1;
        }

        // 严格门禁：只有 Complete 才构造快照并进入选择器
        if (dev_enum.status != EnumerationStatus::Complete) {
            std::cout << "maintenance stage=enumerate_devices status="
                      << enumeration_status_name(dev_enum.status)
                      << " result=" << static_cast<int>(dev_enum.result)
                      << " count="  << dev_enum.count
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 6;
        }

        std::vector<DeviceInfo> snapshots;
        std::vector<std::vector<QueueFamilyInfo>> queue_families;
        snapshots.reserve(dev_enum.count);
        queue_families.reserve(dev_enum.count);
        for (std::size_t i = 0;
             i < dev_enum.count && i < dev_enum.devices.size();
             ++i) {
            snapshots.push_back(
                query_device_info(dev_enum.devices[i], &vkGetPhysicalDeviceProperties));
            queue_families.push_back(
                query_queue_families(dev_enum.devices[i],
                                     &vkGetPhysicalDeviceQueueFamilyProperties));
        }

        std::optional<DeviceSelection> sel =
            select_target_graphics_device(snapshots, queue_families);
        if (!sel.has_value()) {
            std::cout << "maintenance stage=select_device found=0"
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 6;
        }

        VkPhysicalDevice phys = dev_enum.devices[sel->device_index];
        std::uint32_t family_index = sel->queue_family_index;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(phys, &props);

        std::cout << "maintenance stage=selected_device"
                  << " device_index="      << sel->device_index
                  << " family_index="      << family_index
                  << " requested_api="     << VK_API_VERSION_1_0
                  << " apiVersion="        << props.apiVersion
                  << " driverVersion_raw=" << props.driverVersion
                  << " vendorID="          << props.vendorID
                  << " deviceID="          << props.deviceID
                  << " deviceType="        << static_cast<int>(props.deviceType)
                  << " deviceName="        << std::quoted(props.deviceName)
                  << "\n";
                  
        DeviceExtensionEnumeration dev_ext =
            enumerate_device_extensions(phys, &vkEnumerateDeviceExtensionProperties);

        if (dev_ext.result != VK_SUCCESS && dev_ext.result != VK_INCOMPLETE) {
            std::cout << "maintenance stage=device_extensions status="
                      << enumeration_status_name(dev_ext.status)
                      << " result=" << static_cast<int>(dev_ext.result)
                      << " device_index=" << sel->device_index
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 1;
        }

        bool has_swapchain = false, has_maint1 = false;
        bool dev_ext_ok = check_device_extensions(dev_ext, has_swapchain, has_maint1);
        if (!dev_ext_ok) {
            std::cout << "maintenance stage=device_extensions status="
                      << enumeration_status_name(dev_ext.status)
                      << " result=" << static_cast<int>(dev_ext.result)
                      << " device_index=" << sel->device_index
                      << " swapchain="    << has_swapchain
                      << " maintenance1=" << has_maint1
                      << " query_called=0 supported=0 enabled=0\n";
            vkDestroyInstance(instance, nullptr);
            return 6;
        }

        // ---------------- 阶段 5：特性查询 ----------------
        SwapchainMaintenanceQueryConfig qcfg{};
        qcfg.physical_device               = phys;
        qcfg.instance_dependencies_enabled = true;
        qcfg.device_extensions_supported   = true;

        SwapchainMaintenanceSupportAttempt qatt =
            query_swapchain_maintenance1(qcfg, get_features2);

        if (qatt.status != SwapchainMaintenanceSupportStatus::Supported) {
            std::cout << "maintenance stage=query_features status="
                      << maintenance_status_name(qatt.status)
                      << " query_called=" << (qatt.query_called ? 1 : 0)
                      << " supported="    << (qatt.supported ? 1 : 0)
                      << " enabled=0"
                      << " device_index=" << sel->device_index
                      << " family_index=" << family_index << "\n";
            vkDestroyInstance(instance, nullptr);
            return 6;
        }

        // ---------------- 阶段 6：显式设备创建验证 ----------------
        DeviceConfig dev_cfg{};
        dev_cfg.physical_device                 = phys;
        dev_cfg.queue_family_index              = family_index;
        dev_cfg.extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        };
        dev_cfg.enable_swapchain_maintenance1   = true;

        DeviceQueueAttempt dev_q = create_device_with_queue(
            dev_cfg, &vkCreateDevice, &vkGetDeviceQueue);

        if (dev_q.result != VK_SUCCESS || dev_q.device == VK_NULL_HANDLE) {
            // 设备创建失败：特性从未成功激活，enabled 只能为 0
            std::cout << "maintenance stage=create_device result="
                      << static_cast<int>(dev_q.result)
                      << " query_called=" << (qatt.query_called ? 1 : 0)
                      << " supported="    << (qatt.supported ? 1 : 0)
                      << " enabled=0"
                      << " device_index=" << sel->device_index
                      << " family_index=" << family_index << "\n";
            vkDestroyInstance(instance, nullptr);
            return 1;
        }

        // ---------------- 阶段 7：逆序释放与成功退出 ----------------
        std::cout << "maintenance stage=complete result=0"
                  << " query_called=1 supported=1 enabled=1"
                  << " device_index=" << sel->device_index
                  << " family_index=" << family_index << "\n";

        vkDestroyDevice(dev_q.device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return 0;
    }
}


// Initial scaffold prepared by Codex.
int main(int argc, char** argv) {
    // ---- 解析命令行，选择运行模式 ----
    bool window_mode = false;
    const char* projection_path = nullptr;
    const char* scene_path = nullptr;
    const char* scene_save_path = nullptr;
    WindowRenderMode mode = WindowRenderMode::Textured;

    auto is_mvp_or_normal_mode = [](const char* arg, WindowRenderMode& out_mode) -> bool {
        if (std::strcmp(arg, "--window-mvp") == 0) {
            out_mode = WindowRenderMode::MvpTextured;
            return true;
        }
        if (std::strcmp(arg, "--window-normal") == 0) {
            out_mode = WindowRenderMode::NormalDebug;
            return true;
        }
        return false;
    };

    if (argc == 1) {
        // 无参数：window_mode 保持 false，mode 保持默认 Textured
    } else if (argc == 2 && std::strcmp(argv[1], "--window") == 0) {
        window_mode = true;
        mode = WindowRenderMode::Textured;
    } else if (argc == 2 && is_mvp_or_normal_mode(argv[1], mode)) {
        window_mode = true;
    } else if (argc == 4 && is_mvp_or_normal_mode(argv[1], mode) &&
               (std::strcmp(argv[2], "--projection") == 0 ||
                std::strcmp(argv[2], "--scene") == 0 ||
                std::strcmp(argv[2], "--save-scene") == 0)) {
        window_mode = true;
        if (std::strcmp(argv[2], "--projection") == 0) {
            projection_path = argv[3];
        } else if (std::strcmp(argv[2], "--scene") == 0) {
            scene_path = argv[3];
        } else {
            scene_save_path = argv[3];
        }
    } else if (argc == 6 && is_mvp_or_normal_mode(argv[1], mode) &&
               (std::strcmp(argv[2], "--projection") == 0 ||
                std::strcmp(argv[2], "--scene") == 0) &&
               std::strcmp(argv[4], "--save-scene") == 0) {
        window_mode = true;
        if (std::strcmp(argv[2], "--projection") == 0) {
            projection_path = argv[3];
        } else {
            scene_path = argv[3];
        }
        scene_save_path = argv[5];
    } else if (argc == 2 && std::strcmp(argv[1], "--maintenance-check") == 0) {
        return run_maintenance_check();    // 独立早返回，不进入原渲染流
    } else {
        std::cout << "usage: " << argv[0]
                  << " [--window | --window-mvp [--projection <path> | --scene <path>] [--save-scene <path>]"
                  << " | --window-normal [--projection <path> | --scene <path>] [--save-scene <path>]"
                  << " | --maintenance-check]\n";
        return 5;
    }

    InstanceConfig config{};
    config.application_name = "vk01_instance_smoke";
    config.api_version = VK_API_VERSION_1_0;
    // ---- 窗口模式交给独立会话；返回后直接退出 main ----
    if (window_mode) {
        SceneReadResult settings{true, {}, {}};
        if (scene_path != nullptr) {
            settings = load_scene_config(scene_path);
        } else if (projection_path != nullptr) {
            ProjectionReadResult projection_settings =
                load_projection_config(projection_path);
            if (!projection_settings.success) {
                settings.success = false;
                settings.error = projection_settings.error;
            } else {
                settings.config.projection = projection_settings.config;
            }
        }

        if (!settings.success) {
            std::cout << settings.error << "\n";
            return 14;
        }

        if (scene_save_path != nullptr) {
            SceneWriteResult saved =
                save_scene_config(scene_save_path, settings.config);
            if (!saved.success) {
                std::cout << saved.error << "\n";
                return 15;
            }
        }

        MeshLoadResult loaded =
            load_obj_mesh("assets/demo_mesh.obj");
        if (!loaded.success) {
            std::cout << "window stage=mesh_load " << loaded.error << "\n";
            return 13;
        }
        std::cout << "window stage=mesh_load success=1"
                  << " path=assets/demo_mesh.obj"
                  << " vertices=" << loaded.mesh.vertices.size()
                  << " indices=" << loaded.mesh.indices.size()
                  << "\n";
        WindowConfig wconfig{800, 600, "VK01"};
        WindowAttempt wattempt = open_vulkan_window(wconfig);
        if (!wattempt.ok()) {
            const char* stage = (wattempt.stage == WindowAttempt::Stage::GlfwInit)
                             ? "glfw_init" : "create_window";
            print_window_error(stage, wattempt.error);
            return 4;
        }
        return run_window_session(
            wattempt.session, loaded.mesh, mode, settings.config);
    }

    // ---- 无参数诊断：创建实例 ----
    InstanceAttempt instance_attempt = create_instance(config, &vkCreateInstance);
    if (instance_attempt.result != VK_SUCCESS) {
        std::cout << "stage=create_instance result=" << instance_attempt.result
                  << " handle_valid=0 destroy_called=0\n";
        return 1;
    }

    // ---- 枚举物理设备，收集每台设备的属性与队列族快照 ----
    DeviceEnumeration enumeration =
        enumerate_devices(instance_attempt.instance, &vkEnumeratePhysicalDevices);

    std::vector<DeviceInfo> snapshots;
    std::vector<std::vector<QueueFamilyInfo>> queue_family;

    if (enumeration.status == EnumerationStatus::Complete ||
        enumeration.status == EnumerationStatus::Incomplete) {
        for (std::size_t i = 0;
             i < enumeration.count && i < enumeration.devices.size();
             ++i) {
            snapshots.push_back(
                query_device_info(enumeration.devices[i],
                                  &vkGetPhysicalDeviceProperties));
            queue_family.push_back(query_queue_families(enumeration.devices[i], &vkGetPhysicalDeviceQueueFamilyProperties));
        }
    }

    enum class DeviceCreateStatus {
        Skipped,
        NoCandidate,
        CreateFailed,
        CreatedAndReleased
    };

    DeviceCreateStatus device_status = DeviceCreateStatus::Skipped;
    std::size_t device_index_out = 0;
    std::uint32_t family_index_out = 0;
    VkResult device_result_out = VK_SUCCESS;

    // ---- 无参数诊断：选择设备，创建逻辑设备和队列，然后释放 ----
    if (enumeration.status == EnumerationStatus::Complete) {
        std::optional<DeviceSelection> sel =
            select_target_graphics_device(snapshots, queue_family);
        if (!sel) {
            device_status = DeviceCreateStatus::NoCandidate;
        } else {
            DeviceConfig cfg{};
            cfg.physical_device = enumeration.devices[sel->device_index];
            cfg.queue_family_index = sel->queue_family_index;

            device_index_out = sel->device_index;
            family_index_out = sel->queue_family_index;

            DeviceQueueAttempt attempt_device =
                create_device_with_queue(cfg, &vkCreateDevice, &vkGetDeviceQueue);
            device_result_out = attempt_device.result;
            if (attempt_device.result != VK_SUCCESS) {
                device_status = DeviceCreateStatus::CreateFailed;
            } else {
                device_status = DeviceCreateStatus::CreatedAndReleased;
                vkDestroyDevice(attempt_device.device, nullptr);
            }
        }
    }

    // ---- 汇总输出：枚举结果、设备与队列族列表、逻辑设备状态 ----
    const char* stage       = "enumerate";
    const char* status_name = "Unknown";
    uint32_t count_out      = 0;
    int code                = 2;

    // 按物理设备枚举状态映射阶段名与基础退出码（0 完整/为空，2 不完整，1 枚举失败）
    switch (enumeration.status) {
        case EnumerationStatus::Complete:
            stage = "enumerate_devices"; status_name = "Complete";
            count_out = enumeration.count; code = 0; break;
        case EnumerationStatus::Empty:
            stage = "enumerate_devices"; status_name = "Empty";
            count_out = enumeration.count; code = 0; break;
        case EnumerationStatus::Incomplete:
            stage = "enumerate_fill"; status_name = "Incomplete";
            count_out = enumeration.count; code = 2; break;
        case EnumerationStatus::CountFailed:
            stage = "enumerate_count"; status_name = "CountFailed";
            count_out = 0; code = 1; break;
        case EnumerationStatus::FillFailed:
            stage = "enumerate_fill"; status_name = "FillFailed";
            count_out = 0; code = 1; break;
    }

    std::cout << "stage=" << stage
          << " result=" << static_cast<int>(enumeration.result)
          << " status=" << status_name
          << " count=" << count_out
          << " destroy_called=1\n";
    
    std::cout << "devices status=" << status_name
              << " count=" << snapshots.size();
    if (enumeration.status == EnumerationStatus::Incomplete) {
        std::cout << " (partial list)";
    }
    std::cout << "\n";

    // 逐台输出物理设备属性、队列族列表及匹配到的图形队列族
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        std::cout << "device index=" << i
                  << " apiVersion=" << snapshots[i].apiVersion
                  << " vendorID=" << snapshots[i].vendorID
                  << " deviceID=" << snapshots[i].deviceID
                  << " deviceType=" << static_cast<int>(snapshots[i].deviceType)
                  << " deviceName=" << std::quoted(snapshots[i].deviceName)
                  << "\n"
                  << "queues device index=" << i << " count=" << queue_family[i].size() << "\n";
                  for(std::size_t j = 0 ; j < queue_family[i].size(); j++ ){
                     std::cout<< "queue device index=" << i
                              << " familyIndex=" << queue_family[i][j].familyIndex
                              << " queueFlags="  << queue_family[i][j].queueFlags
                              << " queueCount="  << queue_family[i][j].queueCount
                              << "\n";
                    }
        std::optional<std::uint32_t> found =
            find_graphics_queue_family(queue_family[i]);
        std::cout << "graphics device index=" << i;
        if (found.has_value()) {
            std::cout << " found=1 familyIndex=" << *found;
        } else {
            std::cout << " found=0";
        }
        std::cout << "\n";
    }

    // 输出逻辑设备与图形队列的创建状态
    const char* device_status_name = "Skipped";
    switch (device_status) {
        case DeviceCreateStatus::Skipped:
            device_status_name = "Skipped"; break;
        case DeviceCreateStatus::NoCandidate:
            device_status_name = "NoCandidate"; break;
        case DeviceCreateStatus::CreateFailed:
            device_status_name = "CreateFailed"; break;
        case DeviceCreateStatus::CreatedAndReleased:
            device_status_name = "CreatedAndReleased"; break;
    }

    std::cout << "logical_device status=" << device_status_name;
    if (device_status == DeviceCreateStatus::CreateFailed ||
        device_status == DeviceCreateStatus::CreatedAndReleased) {
        std::cout << " attempted=1"
                  << " device_index=" << device_index_out
                  << " familyIndex=" << family_index_out
                  << " result=" << static_cast<int>(device_result_out);
        if (device_status == DeviceCreateStatus::CreateFailed) {
            std::cout << " queue_obtained=0 device_destroy_called=0";
        } else {
            std::cout << " queue_obtained=1 device_destroy_called=1";
        }
    } else {
        std::cout << " attempted=0";
    }
    std::cout << "\n";

    // 枚举的基础退出码保持不变；设备选择或创建失败返回 3。
    if (device_status == DeviceCreateStatus::NoCandidate ||
        device_status == DeviceCreateStatus::CreateFailed) {
        code = 3;
    }

    vkDestroyInstance(instance_attempt.instance, nullptr);
    return code;
}
