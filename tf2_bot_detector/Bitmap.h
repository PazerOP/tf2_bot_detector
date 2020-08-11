#pragma once

#include <memory>
#include <filesystem>

namespace tf2_bot_detector
{
	class Bitmap
	{
	public:
		Bitmap() = default;
		Bitmap(const std::filesystem::path& path);
		Bitmap(const std::filesystem::path& path, uint8_t desiredChannels);

		void LoadFile(const std::filesystem::path& path);
		void LoadFile(const std::filesystem::path& path, uint8_t desiredChannels);

		const void* GetData() const { return m_Image.get(); }
		uint32_t GetHeight() const { return m_Height; }
		uint32_t GetWidth() const { return m_Width; }
		uint8_t GetChannelCount() const { return m_Channels; }

		bool empty() const { return !m_Image; }

	private:
		struct Deleter final
		{
			void operator()(void* ptr) const;
		};
		std::unique_ptr<std::byte, Deleter> m_Image;
		uint32_t m_Width{};
		uint32_t m_Height{};
		uint8_t m_Channels{};
	};
}
