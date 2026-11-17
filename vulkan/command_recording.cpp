#include "command_recording.h"

CommandPoolAttempt create_graphics_command_pool(
    VkDevice device, std::uint32_t queue_family_index,
    PFN_vkCreateCommandPool create_fn){
        CommandPoolAttempt out;

        VkCommandPoolCreateInfo ci{};
        ci.flags = 0;
        ci.pNext = nullptr;
        ci.sType = 	VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = queue_family_index;

        out.result = create_fn (device, &ci, nullptr, &out.pool);

        if(out.result != VK_SUCCESS){
            out.pool = VK_NULL_HANDLE;
            return out;
        }
        return out;
    }

CommandRecordingAttempt record_triangle_command_buffers(
    const TriangleRecordingConfig& config, const CommandRecordingApi& api){
        std::vector<VkCommandBuffer> slots;
        CommandRecordingAttempt out;
        if(config.mvp.enabled == true && 
          (api.push_constants == nullptr || config.pipeline_layout == VK_NULL_HANDLE))
        { return out; }

        std::uint32_t N = static_cast<std::uint32_t>(config.framebuffers.size());
        slots.resize(N);

        VkCommandBufferAllocateInfo ai{};
        ai.commandBufferCount = N;
        ai.commandPool = config.pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.pNext = nullptr;
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        out.result = api.allocate(config.device, &ai, slots.data());
        if (out.result != VK_SUCCESS) {
            out.allocated_count = 0;
            out.recorded_count = 0;
            return out;
        }
        out.allocated_count = N;

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.pNext = nullptr;
        bi.flags = 0;
        bi.pInheritanceInfo = nullptr;

        for (std::uint32_t i = 0; i < N; ++i) {
            VkCommandBuffer cb = slots[i];

            out.result = api.begin(cb, &bi);
            if (out.result != VK_SUCCESS) {
                out.recorded_count = i;
                return out;
            }

            VkClearValue clear_values[2]{};
            clear_values[0].color = {{0.05f, 0.05f, 0.10f, 1.0f}};
            clear_values[1].depthStencil = {1.0f, 0};

            VkRenderPassBeginInfo rpbi{};
            rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpbi.pNext = nullptr;
            rpbi.renderPass = config.render_pass;
            rpbi.framebuffer = config.framebuffers[i];
            rpbi.renderArea.offset = {0, 0};
            rpbi.renderArea.extent = config.extent;
            rpbi.clearValueCount = config.depth_enabled ? 2 : 1;
            rpbi.pClearValues = clear_values;

            api.begin_render_pass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            api.bind_pipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, config.pipeline);
            if(config.mvp.enabled == true){
                api.push_constants(
                    cb,
                    config.pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,sizeof(TransformPush),	
                &config.mvp.push);
            }
            if (config.texture_set != VK_NULL_HANDLE) {
                api.bind_descriptor_sets(
                    cb,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    config.pipeline_layout,
                    0, 1, &config.texture_set,
                    0, nullptr);
            }
            if(config.material_set != VK_NULL_HANDLE){
                api.bind_descriptor_sets(
                    cb, 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    config.pipeline_layout,
                    1, 1, &config.material_set, 
                    0, nullptr);
            }
            if(config.vertex_buffer != VK_NULL_HANDLE){
                VkBuffer vb = config.vertex_buffer;
                VkDeviceSize vb_offset = 0;
                api.bind_vertex_buffers(
                    cb, 0, 1, &vb, &vb_offset);
            }
            if (config.index_buffer != VK_NULL_HANDLE) { 
                api.bind_index_buffer(cb, config.index_buffer, 0, config.index_type); 
                api.draw_indexed(cb, config.index_count, 1, 0, 0, 0); 
            } else { api.draw(cb, 3, 1, 0, 0); }
            api.end_render_pass(cb);

            out.result = api.end(cb);
            if (out.result != VK_SUCCESS) {
                out.recorded_count = i;
                return out;
            }
            ++out.recorded_count;
        }

        out.buffers = std::move(slots);
        return out;
    }
