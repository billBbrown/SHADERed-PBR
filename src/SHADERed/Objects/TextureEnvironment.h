#pragma once
#include <gl/glew.h>
#include <string>

namespace ed {
	namespace TextureEnvironment {
		struct Texture {
			GLenum target = GL_TEXTURE_2D;
			GLuint id = 0;
			int width = 0, height = 0;
			int levels = 1;
			GLenum format = GL_RGB;
			GLenum type = GL_UNSIGNED_BYTE;
			GLenum internalFormat = GL_RGB8;
			int GetChannelSize();
			int GetChannelCount();
			bool IsFloatPixel();
			bool Validate() { return GetChannelCount() > 0 && GetChannelSize() > 0; }
			bool HasAlpha() { return GetChannelCount() == 4; }

			static int GetChannelSize(GLenum type);
			static int GetChannelCount(GLenum format);
			static bool IsFloatPixel(GLenum type);
		};

		struct EnvironmentTexture {
			Texture m_originTexture;
			Texture m_envTexture;
			Texture m_irmapTexture;
			Texture m_spBRDF_LUT;
			bool m_valid = false;
		};

		EnvironmentTexture Create(const std::string& file);
		bool SaveTextureToFile(Texture cubeTexture, const std::string& fileName); //save the texture to file ; if its a float texture, extension will be forcely changed to .hdr
	}
}