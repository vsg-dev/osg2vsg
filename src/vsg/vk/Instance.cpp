#include <vsg/vk/Instance.h>

#include <set>
#include <iostream>

namespace vsg
{

Names validateInstancelayerNames(const Names& names)
{
    if (names.empty()) return names;

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    using NameSet = std::set<std::string>;
    NameSet layerNames;
    for (const auto& layer : availableLayers)
    {
        std::cout<<"layer="<<layer.layerName<<std::endl;
        if (layer.layerName) layerNames.insert(layer.layerName);
    }

    Names validatedNames;
    validatedNames.reserve(names.size());
    for (const auto& requestedName : names)
    {
        if (layerNames.count(requestedName)!=0)
        {
            std::cout<<"valid requested layer : "<<requestedName<<std::endl;
            validatedNames.push_back(requestedName);
        }
        else
        {
            std::cout<<"Warning : requested invalid layer : "<<requestedName<<std::endl;
        }
    }

    return validatedNames;
}

Instance::Instance(Names& instanceExtensions, Names& layers, AllocationCallbacks* allocator):
    _instance(VK_NULL_HANDLE),
    _allocator(allocator)
{
    // applictin info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Test";
    appInfo.pEngineName = "VulkanSceneGraph";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.empty() ? nullptr : instanceExtensions.data();

    createInfo.enabledLayerCount = layers.size();
    createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    if (vkCreateInstance(&createInfo, *_allocator, &_instance) == VK_SUCCESS)
    {
        std::cout<<"Created VkInstance"<<std::endl;
    }
    else
    {
        std::cout<<"Failed to create VkInstance"<<std::endl;
    }
}

Instance::~Instance()
{
    if (_instance)
    {
        std::cout<<"Calling vkDestroyInstance"<<std::endl;
        vkDestroyInstance(_instance, *_allocator);
    }
}

} // end of namespace vsg