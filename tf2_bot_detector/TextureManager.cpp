#include "TextureManager.h"
#include "Bitmap.h"

#include <glbinding-aux/ContextInfo.h>
#include <glbinding/gl21/gl.h>
#include <glbinding/gl21ext/gl.h>
#include <glbinding/gl30/gl.h>
#include <mh/memory/unique_object.hpp>

using namespace gl21;
using namespace tf2_bot_detector;

namespace
{
	struct TextureHandleTraits final
	{
		static void delete_obj(GLuint t)
		{
			glDeleteTextures(1, &t);
		}
		static GLuint release_obj(GLuint& t)
		{
			auto retVal = t;
			t = {};
			return retVal;
		}
		static bool is_obj_valid(GLuint t)
		{
			return t > 0;
		}
	};

	using TextureHandle = mh::unique_object<GLuint, TextureHandleTraits>;

	class TextureManager;

	class Texture final : public ITexture
	{
	public:
		Texture(const TextureManager& manager, const Bitmap& bitmap);

		handle_type GetHandle() const override { return m_Handle; }

	private:
		TextureHandle m_Handle{};
	};

	class TextureManager final : public ITextureManager
	{
	public:
		TextureManager();

		void EndFrame() override;
		std::shared_ptr<ITexture> CreateTexture(const Bitmap& bitmap) override;
		size_t GetActiveTextureCount() const override { return m_Textures.size(); }

		bool HasExtension(GLextension ext) const { return GetExtensions().contains(ext); }
		const std::set<GLextension>& GetExtensions() const { return m_Extensions; }

		const glbinding::Version& GetContextVersion() const { return m_ContextVersion; }

	private:
		glbinding::Version m_ContextVersion{};

		const std::set<GLextension> m_Extensions = glbinding::aux::ContextInfo::extensions();
		uint64_t m_FrameCount{};
		std::vector<std::shared_ptr<Texture>> m_Textures;
	};
}

std::unique_ptr<ITextureManager> tf2_bot_detector::CreateTextureManager()
{
	return std::make_unique<TextureManager>();
}

TextureManager::TextureManager()
{
	m_ContextVersion = glbinding::aux::ContextInfo::version();
}

void TextureManager::EndFrame()
{
	std::erase_if(m_Textures, [](const std::shared_ptr<Texture>& t)
		{
			return t.use_count() == 1;
		});
}

std::shared_ptr<ITexture> TextureManager::CreateTexture(const Bitmap& bitmap)
{
	return m_Textures.emplace_back(std::make_shared<Texture>(*this, bitmap));
}

Texture::Texture(const TextureManager& manager, const Bitmap& bitmap)
{
	GLenum internalFormat{};
	GLenum sourceFormat{};
	std::array<GLenum, 4> swizzle{};
	constexpr GLenum sourceType = GL_UNSIGNED_BYTE;
	switch (bitmap.GetChannelCount())
	{
	case 1:
		internalFormat = GL_RED;
		sourceFormat = GL_RED;
		swizzle = { GL_RED, GL_RED, GL_RED, GL_ONE };
		break;
	case 2:
		internalFormat = GL_RGB;
		sourceFormat = GL_RGB;
		swizzle = { GL_RED, GL_RED, GL_RED, GL_GREEN };
		break;
	case 3:
		internalFormat = GL_RGB;
		sourceFormat = GL_RGB;
		swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE };
		break;
	case 4:
		internalFormat = GL_RGBA;
		sourceFormat = GL_RGBA;
		swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		break;
	}

	glGenTextures(1, &m_Handle.reset_and_get_ref());
	glBindTexture(GL_TEXTURE_2D, m_Handle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, bitmap.GetWidth(), bitmap.GetHeight(), 0,
		sourceFormat, sourceType, bitmap.GetData());

	if (manager.HasExtension(GLextension::GL_ARB_texture_swizzle))
	{
		using namespace gl21ext;
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle.data());
	}
	else if (manager.HasExtension(GLextension::GL_EXT_texture_swizzle))
	{
		using namespace gl21ext;
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA_EXT, swizzle.data());
	}

	if (manager.GetContextVersion() >= glbinding::Version(3, 0))
	{
		gl30::glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		// just disable mipmaps
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
}
