#include "BaseTextures.h"
#include "Bitmap.h"
#include "Log.h"
#include "TextureManager.h"

#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <string>

using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	class BaseTextures final : public IBaseTextures
	{
	public:
		BaseTextures(ITextureManager& textureManager);

		const ITexture* GetHeart_16() const override { return m_Heart_16.get(); }
		const ITexture* GetVACShield_16() const override { return m_VACShield_16.get(); }

	private:
		ITextureManager& m_TextureManager;

		std::shared_ptr<ITexture> m_Heart_16;
		std::shared_ptr<ITexture> m_VACShield_16;

		std::shared_ptr<ITexture> TryLoadTexture(const std::filesystem::path& file) const;
	};
}

std::unique_ptr<IBaseTextures> IBaseTextures::Create(ITextureManager& textureManager)
{
	return std::make_unique<BaseTextures>(textureManager);
}

BaseTextures::BaseTextures(ITextureManager& textureManager) :
	m_TextureManager(textureManager),

	m_Heart_16(TryLoadTexture("images/heart_16.png")),
	m_VACShield_16(TryLoadTexture("images/vac_icon_16.png"))
{
}

std::shared_ptr<ITexture> BaseTextures::TryLoadTexture(const std::filesystem::path& file) const
{
	try
	{
		return m_TextureManager.CreateTexture(Bitmap(file));
	}
	catch (const std::exception& e)
	{
		LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to load "s << file, e);
	}

	return nullptr;
}
