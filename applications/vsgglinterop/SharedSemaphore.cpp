#include "SharedSemaphore.h"

using namespace vsg;

ref_ptr<SharedSemaphore> vsg::createSharedSemaphore(Device* device)
{
    VkExportSemaphoreCreateInfo esci;
    esci.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    esci.pNext = nullptr;
    esci.handleTypes = SharedSemaphore::kSemaphoreHandleType;

    ref_ptr<SharedSemaphore> semaphore = SharedSemaphore::create(device, &esci);

#if WIN32
    auto vkGetSemaphoreHandle = PFN_vkGetSemaphoreWin32HandleKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetSemaphoreWin32HandleKHR"));

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
#else
    auto vkGetSemaphoreHandle = PFN_vkGetSemaphoreFdKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetSemaphoreFdKHR"));

    VkSemaphoreGetFdHandleInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
#endif

    getHandleInfo.pNext = nullptr;
    getHandleInfo.handleType = SharedSemaphore::kSemaphoreHandleType;
    getHandleInfo.semaphore = *semaphore;

    SharedSemaphore::HandleType handle = 0;
    VkResult result = vkGetSemaphoreHandle(device->getDevice(), &getHandleInfo, &handle);

    semaphore->_handle = handle;
    return semaphore;
}
