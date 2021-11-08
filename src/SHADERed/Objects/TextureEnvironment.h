#pragma once
#include <gl/glew.h>
#include <string>

namespace TextureEnvironment 
{
	struct Texture {
		Texture()
				: id(0)
		{
		}
		GLuint id;
		int width, height;
		int levels;
		GLenum format;
		GLenum type;
		GLenum internalFormat;
	};

	struct EnvironmentTexture {
		Texture m_originTexture;
		Texture m_envTexture;
		Texture m_irmapTexture;
		Texture m_spBRDF_LUT;
		bool m_valid = false;
	};

	EnvironmentTexture Create(const std::string& file);
}