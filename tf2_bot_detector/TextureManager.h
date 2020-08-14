#pragma once

#include <memory>

namespace tf2_bot_detector
{
	class Bitmap;

	struct TextureSettings
	{
		bool m_EnableMips = false;
	};

	class ITexture
	{
	public:
		using handle_type = uint32_t;

		virtual ~ITexture() = default;

		virtual handle_type GetHandle() const = 0;
		virtual const TextureSettings& GetSettings() const = 0;

		virtual uint16_t GetWidth() const = 0;
		virtual uint16_t GetHeight() const = 0;
	};

	class ITextureManager
	{
	public:
		virtual ~ITextureManager() = default;

		virtual void EndFrame() = 0;

		virtual std::shared_ptr<ITexture> CreateTexture(const Bitmap& bitmap,
			const TextureSettings& settings = {}) = 0;

		virtual size_t GetActiveTextureCount() const = 0;
	};

	std::unique_ptr<ITextureManager> CreateTextureManager();
}
