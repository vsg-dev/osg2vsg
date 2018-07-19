#pragma once

#include <vsg/vk/Device.h>
#include <vsg/vk/Buffer.h>

namespace vsg
{
    class DeviceMemory : public Object
    {
    public:
        DeviceMemory(Device* device, VkDeviceMemory DeviceMemory, AllocationCallbacks* allocator=nullptr);

        using Result = vsg::Result<DeviceMemory, VkResult, VK_SUCCESS>;
        static Result create(PhysicalDevice* physicalDevice, Device* device, VkMemoryRequirements& memRequirements, VkMemoryPropertyFlags properties, AllocationCallbacks* allocator=nullptr);

        static Result create(PhysicalDevice* physicalDevice, Device* device, Buffer* buffer, VkMemoryPropertyFlags properties, AllocationCallbacks* allocator=nullptr)
        {
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(*device, *buffer, &memRequirements);

            return vsg::DeviceMemory::create(physicalDevice, device, memRequirements, properties, allocator);
        }


        void copy(VkDeviceSize offset, VkDeviceSize size, void* src_data);
        void copy(VkDeviceSize offset, Data* data);

        operator VkDeviceMemory () const { return _deviceMemory; }

    protected:
        virtual ~DeviceMemory();

        ref_ptr<Device>                 _device;
        VkDeviceMemory                  _deviceMemory;
        ref_ptr<AllocationCallbacks>    _allocator;
    };

}