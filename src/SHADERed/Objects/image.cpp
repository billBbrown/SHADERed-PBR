/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 */

#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION 
#include <stb_image_write.h>

#include "image.hpp"
#include <filesystem>

Image::Image()
	: m_width(0)
	, m_height(0)
	, m_channels(0)
	, m_hdr(false)
{}

std::shared_ptr<Image> Image::fromFile(const std::string& filename, int channels)
{
	std::printf("Loading image: %s\n", filename.c_str());

	stbi_set_flip_vertically_on_load(0);

	std::shared_ptr<Image> image{new Image};

	if(stbi_is_hdr(filename.c_str())) {
		float* pixels = stbi_loadf(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels, channels);
		if(pixels) {
			image->m_pixels.reset(reinterpret_cast<unsigned char*>(pixels));
			image->m_hdr = true;
		}
	}
	else {
		unsigned char* pixels = stbi_load(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels, channels);
		if(pixels) {
			image->m_pixels.reset(pixels);
			image->m_hdr = false;
		}
	}
	if(channels > 0) {
		image->m_channels = channels;
	}

	if(!image->m_pixels) {
		return nullptr;
	}
	return image;
}

bool Image::writeFile(const std::string& filename, int width, int height, int channels, int strideByteOfARow, void* data)
{
	stbi_set_flip_vertically_on_load(0);

	std::string ext = std::filesystem::path(filename).extension().string();
	if (ext == ".bmp") {
		return !!stbi_write_bmp(filename.c_str(), width, height, channels, data);
	} else if (ext == ".jpg") {
		constexpr int quality = 100;
		return !!stbi_write_jpg(filename.c_str(), width, height, channels, data, quality);
	} else if (ext == ".png") {
		return !!stbi_write_png(filename.c_str(), width, height, channels, data, strideByteOfARow);
	} else if (ext == ".tga") {
		return !!stbi_write_tga(filename.c_str(), width, height, channels, data);
	} else if (ext == ".hdr") {
		return !!stbi_write_hdr(filename.c_str(), width, height, channels, (float*)data);
	} else {
		assert("Unsupported");
		return false;
	}
}
