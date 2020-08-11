#include "Bitmap.h"

#include <mh/text/string_insertion.hpp>

#define STBI_FAILURE_USERMSG 1
#define STB_IMAGE_IMPLEMENTATION 1
#include <stb_image.h>

using namespace tf2_bot_detector;
using namespace std::string_literals;

void Bitmap::Deleter::operator()(void* ptr) const
{
	stbi_image_free(ptr);
}

Bitmap::Bitmap(const std::filesystem::path& path)
{
	LoadFile(path);
}

Bitmap::Bitmap(const std::filesystem::path& path, uint8_t desiredChannels)
{
	LoadFile(path, desiredChannels);
}

void Bitmap::LoadFile(const std::filesystem::path& path)
{
	return LoadFile(path, 0);
}

void Bitmap::LoadFile(const std::filesystem::path& path, uint8_t desiredChannels)
{
	int width, height, channels;
	m_Image.reset(reinterpret_cast<std::byte*>(
		stbi_load(path.string().c_str(), &width, &height, &channels, desiredChannels)));

	m_Width = width;
	m_Height = height;
	m_Channels = channels;

	if (!m_Image)
		throw std::runtime_error("Failed to load image from "s << path << ": " << stbi_failure_reason());
}
