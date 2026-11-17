#include "render_targets.h"

RenderPassAttempt create_color_render_pass(const ColorRenderPassConfig& config,
                                           PFN_vkCreateRenderPass create_fn) {
    const bool has_depth = config.depth_format != VK_FORMAT_UNDEFINED;
    RenderPassAttempt attempt{};

    // 颜色附件：开始时清屏，结束时保存结果并转为可呈现布局。
    VkAttachmentDescription attachment{};
    attachment.format         = config.color_format;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format         = config.depth_format;
    depth_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_ref;
    if (has_depth) {
        subpass.pDepthStencilAttachment = &depth_ref;
    }

    // 外部依赖：等到颜色附件输出阶段（交换链图像已可用）再写入颜色附件。
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (has_depth) {
        const VkPipelineStageFlags depth_stages =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

        dependency.srcStageMask |= depth_stages;
        dependency.dstStageMask |= depth_stages;
        dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkAttachmentDescription attachments[2] = {attachment, depth_attachment};

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.pNext           = nullptr;
    ci.flags           = 0;
    ci.attachmentCount = has_depth ? 2 : 1;
    ci.pAttachments    = attachments;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dependency;

    attempt.result = create_fn(config.device, &ci, nullptr,
                               &attempt.render_pass);

    // 失败时驱动可能已经往输出槽里写过垃圾，必须清掉。
    if (attempt.result != VK_SUCCESS) {
        attempt.render_pass = VK_NULL_HANDLE;
    }

    return attempt;
}

FramebufferAttempt create_color_framebuffer(const FramebufferConfig& config,
                                            PFN_vkCreateFramebuffer create_fn){
    FramebufferAttempt attempt{};
    const bool has_depth = config.depth_view != VK_NULL_HANDLE;
    VkImageView attachments[2] = {config.color_view, config.depth_view};
    VkFramebufferCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.pNext         = nullptr;
    ci.flags         = 0;
    ci.renderPass	 = config.render_pass;
    ci.attachmentCount = has_depth ? 2 : 1;
    ci.pAttachments = attachments;
    ci.width             = config.extent.width;
    ci.height            = config.extent.height;
    ci.layers	         = 1;
    
    attempt.result = create_fn(config.device, &ci, nullptr,
                               &attempt.framebuffer);

    // 失败时驱动可能已经往输出槽里写过垃圾，必须清掉。
    if (attempt.result != VK_SUCCESS) {
        attempt.framebuffer = VK_NULL_HANDLE;
    }

    return attempt;
}