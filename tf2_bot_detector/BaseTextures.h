#pragma once

#include <memory>

namespace tf2_bot_detector
{
	class ITextureManager;
	class ITexture;

	class IBaseTextures
	{
	public:
		virtual ~IBaseTextures() = default;

		static std::unique_ptr<IBaseTextures> Create(std::shared_ptr<ITextureManager> textureManager);

		virtual const ITexture* GetHeart_16() const = 0;
		virtual const ITexture* GetVACShield_16() const = 0;
		virtual const ITexture* GetGameBanIcon_16() const = 0;
	};
}
