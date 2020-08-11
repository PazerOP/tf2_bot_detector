#pragma once

#include <memory>

namespace tf2_bot_detector
{
	class Bitmap;

	class ITexture
	{
	public:
		using handle_type = uint32_t;

		virtual ~ITexture() = default;

		virtual handle_type GetHandle() const = 0;
	};

	class ITextureManager
	{
	public:
		virtual ~ITextureManager() = default;

		virtual void EndFrame() = 0;

		virtual std::shared_ptr<ITexture> CreateTexture(const Bitmap& bitmap) = 0;

		virtual size_t GetActiveTextureCount() const = 0;
	};

	std::unique_ptr<ITextureManager> CreateTextureManager();
}
