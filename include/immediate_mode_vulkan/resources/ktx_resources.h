#include <memory>

#include <ktxvulkan.h>

#include "vulkan_resources.h"

namespace imv {
    inline void check(ktx_error_code_e result) {
        if (result != KTX_SUCCESS) {
            throw std::runtime_error("KTX error");
        }
    }

    struct ktx_device_deleter {
        typedef ktxVulkanDeviceInfo* pointer;
        void operator()(ktxVulkanDeviceInfo* device) {
            ktxVulkanDeviceInfo_Destruct(device);
        }
    };

    struct ktx_texture2_deleter {
        typedef ktxTexture2* pointer;
        void operator()(ktxTexture2* texture) {
            ktxTexture2_Destroy(texture);
        }
    };

    struct ktx_vulkan_texture_deleter {
        typedef ktxVulkanTexture* pointer;
        void operator()(ktxVulkanTexture* texture) {
            ktxVulkanTexture_Destruct(texture, current_device, nullptr);
        }
    };

    typedef std::unique_ptr<ktxVulkanDeviceInfo, ktx_device_deleter> 
        unique_ktx_device;
    typedef std::unique_ptr<ktxTexture2, ktx_texture2_deleter> 
        unique_ktx_texture2;
    typedef std::unique_ptr<ktxVulkanTexture, ktx_vulkan_texture_deleter>
        unique_ktx_vulkan_texture;
}
