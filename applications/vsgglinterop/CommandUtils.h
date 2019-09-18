#pragma once

#include <vulkan/vulkan.h>

#include <vsg/all.h>

namespace vsg
{
    vsg::ref_ptr<vsg::CommandBuffer> gCommandBuffer;

    template<typename F>
    void dispatchCommandsToQueue(vsg::Device* device, vsg::CommandPool* commandPool, std::vector<VkSemaphore> waits, std::vector<VkPipelineStageFlags> waitStages, std::vector<VkSemaphore> signals, VkQueue queue, F function)
    {
        if (!gCommandBuffer) gCommandBuffer = vsg::CommandBuffer::create(device, commandPool, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = gCommandBuffer->flags();

        vkBeginCommandBuffer(*gCommandBuffer, &beginInfo);

        function(*gCommandBuffer);

        vkEndCommandBuffer(*gCommandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = gCommandBuffer->data();
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waits.size());
        submitInfo.pWaitSemaphores = waits.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signals.size());
        submitInfo.pSignalSemaphores = signals.data();

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    }

    void populateSetLayoutCommands(VkCommandBuffer commandBuffer, vsg::Image* image, VkAccessFlagBits srcAccessFlags, VkAccessFlagBits dstAccessFlags, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlagBits srcStageFlags, VkPipelineStageFlagBits dstStageFlags, VkImageAspectFlags aspectFlags)
    {
        vsg::ImageMemoryBarrier imageMemoryBarrier(
            srcAccessFlags, dstAccessFlags,
            srcLayout, dstLayout,
            image);

        imageMemoryBarrier.subresourceRange.aspectMask = aspectFlags;

        imageMemoryBarrier.cmdPiplineBarrier(commandBuffer,
            srcStageFlags, dstStageFlags);
    }
}
