#include "TextureLoader.hpp"
#include <spng.h>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <spdlog/spdlog.h>

namespace VoxelEngine {

TextureData TextureLoader::loadPNG(const std::string& filepath) {
    TextureData result;

    // Open file
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open texture file: " + filepath);
    }

    // Read entire file into buffer
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    // Create spng context
    spng_ctx* ctx = spng_ctx_new(0);
    if (!ctx) {
        throw std::runtime_error("Failed to create spng context");
    }

    // Set PNG buffer
    int ret = spng_set_png_buffer(ctx, fileData.data(), fileSize);
    if (ret) {
        spng_ctx_free(ctx);
        throw std::runtime_error("Failed to set PNG buffer: " + std::string(spng_strerror(ret)));
    }

    // Get image header
    struct spng_ihdr ihdr;
    ret = spng_get_ihdr(ctx, &ihdr);
    if (ret) {
        spng_ctx_free(ctx);
        throw std::runtime_error("Failed to get PNG header: " + std::string(spng_strerror(ret)));
    }

    result.width = ihdr.width;
    result.height = ihdr.height;

    // Determine output format (always RGBA8)
    int fmt = SPNG_FMT_RGBA8;
    result.channels = 4;

    // Get decoded image size
    size_t imageSize;
    ret = spng_decoded_image_size(ctx, fmt, &imageSize);
    if (ret) {
        spng_ctx_free(ctx);
        throw std::runtime_error("Failed to get decoded image size: " + std::string(spng_strerror(ret)));
    }

    // Allocate buffer for decoded image
    result.pixels.resize(imageSize);

    // Decode image - use SPNG_DECODE_TRNS to decode transparency for palette images
    ret = spng_decode_image(ctx, result.pixels.data(), imageSize, fmt, SPNG_DECODE_TRNS);
    if (ret) {
        spng_ctx_free(ctx);
        throw std::runtime_error("Failed to decode PNG image: " + std::string(spng_strerror(ret)));
    }

    spng_ctx_free(ctx);

    spdlog::info("[TextureLoader] Loaded PNG: {} ({}x{})",
                 filepath, result.width, result.height);

    return result;
}

uint32_t TextureLoader::calculateMipLevels(uint32_t width, uint32_t height, uint32_t maxMipLevels) {
    // Calculate maximum possible mip levels for this texture size
    uint32_t maxPossible = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    // Minecraft-style mipmap level limiting:
    // Setting 0 = OFF (1 level, no mipmaps)
    // Setting 1 = 2 mip levels
    // Setting 2 = 3 mip levels
    // Setting 3 = 4 mip levels
    // Setting 4 = 5 mip levels (default)
    // Setting > 4 or 0 = unlimited (use maxPossible)
    if (maxMipLevels >= 1 && maxMipLevels <= 4) {
        uint32_t requestedLevels = maxMipLevels + 1;
        return std::min(requestedLevels, maxPossible);
    }

    // 0 or > 4 means unlimited - use full mip chain
    return maxPossible;
}

void TextureLoader::generateMipmaps(
    VkCommandBuffer cmd,
    VkImage image,
    VkFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels
) {
    // Transition first mip level to TRANSFER_SRC for reading
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = image;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; i++) {
        // Transition current mip level to TRANSFER_DST
        barrier.subresourceRange.baseMipLevel = i;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Blit from previous mip level
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        // Use LINEAR filter for proper averaging when downsampling
        // This approximates gamma-correct blending on the GPU
        vkCmdBlitImage(cmd,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        // Transition current mip level to TRANSFER_SRC for next iteration
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition all mip levels to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

Texture TextureLoader::createTexture(
    VkDevice device,
    VmaAllocator allocator,
    VkCommandBuffer uploadCmd,
    const TextureData& data,
    bool generateMipmaps,
    uint32_t maxMipLevels
) {
    if (!data.isValid()) {
        throw std::runtime_error("Invalid texture data");
    }

    Texture texture;
    texture.width = data.width;
    texture.height = data.height;
    texture.format = VK_FORMAT_R8G8B8A8_SRGB;
    texture.mipLevels = generateMipmaps ? calculateMipLevels(data.width, data.height, maxMipLevels) : 1;

    // Create staging buffer
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = data.pixels.size();
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingInfo;
    vmaCreateBuffer(allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, &stagingInfo);

    // Copy pixel data to staging buffer
    memcpy(stagingInfo.pMappedData, data.pixels.data(), data.pixels.size());

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = texture.format;
    imageInfo.extent = {data.width, data.height, 1};
    imageInfo.mipLevels = texture.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (generateMipmaps) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Needed for mipmap generation
    }
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator, &imageInfo, &imageAllocInfo, &texture.image, &texture.allocation, nullptr);

    // Transition image to TRANSFER_DST for upload
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = texture.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(uploadCmd, &depInfo);

    // Copy buffer to image (first mip level)
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {data.width, data.height, 1};

    vkCmdCopyBufferToImage(uploadCmd, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Store staging buffer for cleanup after submit
    texture.stagingBuffer = stagingBuffer;
    texture.stagingAllocation = stagingAllocation;

    if (generateMipmaps) {
        TextureLoader::generateMipmaps(uploadCmd, texture.image, texture.format, data.width, data.height, texture.mipLevels);
    } else {
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        vkCmdPipelineBarrier2(uploadCmd, &depInfo);
    }

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = texture.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = texture.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView) != VK_SUCCESS) {
        texture.cleanup(device, allocator);
        throw std::runtime_error("Failed to create texture image view");
    }

    return texture;
}

} // namespace VoxelEngine