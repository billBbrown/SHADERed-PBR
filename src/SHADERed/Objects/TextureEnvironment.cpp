/*
* Physically Based Rendering
* Copyright (c) 2017-2018 Michał Siejak
*
* OpenGL 4.5 renderer.
*/

#include "TextureEnvironment.h"
#include "image.hpp"

#include <GLFW/glfw3.h>
#include <gl/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace TextureEnvironment 
{
	static constexpr int kEnvMapSize = 1024;
	static constexpr int kIrradianceMapSize = 32;
	static constexpr int kBRDF_LUT_Size = 256;

	class Utility {
	public:
		template <typename T>
		static constexpr bool isPowerOfTwo(T value)
		{
			return value != 0 && (value & (value - 1)) == 0;
		}
		template <typename T>
		static constexpr T roundToPowerOfTwo(T value, int POT)
		{
			return (value + POT - 1) & -POT;
		}
		template <typename T>
		static constexpr T numMipmapLevels(T width, T height)
		{
			T levels = 1;
			while ((width | height) >> levels) {
				++levels;
			}
			return levels;
		}
	};

	class File {
	public:
		static std::string readText(const std::string& filename)
		{
			std::ifstream file { filename };
			if (!file.is_open()) {
				throw std::runtime_error("Could not open file: " + filename);
			}

			std::stringstream buffer;
			buffer << file.rdbuf();
			return buffer.str();
		}
	};

	class Renderer {
	public:
		struct Capacity {
			float maxAnisotropy = 1.0f;
		};

		static Capacity m_capabilities;

		static Texture createTexture(GLenum target, int width, int height, GLenum internalformat, int levels = 0) 
		{
			Texture texture;
			texture.width = width;
			texture.height = height;
			texture.levels = (levels > 0) ? levels : Utility::numMipmapLevels(width, height);

			glCreateTextures(target, 1, &texture.id);
			glTextureStorage2D(texture.id, texture.levels, internalformat, width, height);
			glTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER, texture.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameterf(texture.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_capabilities.maxAnisotropy);
			return texture;
		}
		static Texture createTexture(const std::shared_ptr<class Image>& image, GLenum format, GLenum internalformat, int levels = 0) 
		{
			Texture texture = createTexture(GL_TEXTURE_2D, image->width(), image->height(), internalformat, levels);
			if (image->isHDR()) {
				glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, GL_FLOAT, image->pixels<float>());
			} else {
				glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, GL_UNSIGNED_BYTE, image->pixels<unsigned char>());
			}

			if (texture.levels > 1) {
				glGenerateTextureMipmap(texture.id);
			}
			return texture;
		}

		static GLuint compileShader(const std::string& filename, GLenum type)
		{
			const std::string src = File::readText(filename);
			if (src.empty()) {
				throw std::runtime_error("Cannot read shader source file: " + filename);
			}
			const GLchar* srcBufferPtr = src.c_str();

			std::printf("Compiling GLSL shader: %s\n", filename.c_str());

			GLuint shader = glCreateShader(type);
			glShaderSource(shader, 1, &srcBufferPtr, nullptr);
			glCompileShader(shader);

			GLint status;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (status != GL_TRUE) {
				GLsizei infoLogSize;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogSize);
				std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
				glGetShaderInfoLog(shader, infoLogSize, nullptr, infoLog.get());
				throw std::runtime_error(std::string("Shader compilation failed: ") + filename + "\n" + infoLog.get());
			}
			return shader;
		}
		static GLuint linkProgram(std::initializer_list<GLuint> shaders)
		{
			GLuint program = glCreateProgram();

			for (GLuint shader : shaders) {
				glAttachShader(program, shader);
			}
			glLinkProgram(program);
			for (GLuint shader : shaders) {
				glDetachShader(program, shader);
				glDeleteShader(shader);
			}

			GLint status;
			glGetProgramiv(program, GL_LINK_STATUS, &status);
			if (status == GL_TRUE) {
				glValidateProgram(program);
				glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
			}
			if (status != GL_TRUE) {
				GLsizei infoLogSize;
				glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogSize);
				std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
				glGetProgramInfoLog(program, infoLogSize, nullptr, infoLog.get());
				throw std::runtime_error(std::string("Program link failed\n") + infoLog.get());
			}
			return program;
		}
	};

	Renderer::Capacity Renderer::m_capabilities;

	EnvironmentTexture Create(const std::string& file)
	{
		EnvironmentTexture et;

		// Unfiltered environment cube map (temporary).
		Texture envTextureUnfiltered = Renderer::createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);

		// Load & convert equirectangular environment map to a cubemap texture.
		{
			GLuint equirectToCubeProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/equirect2cube_cs.glsl", GL_COMPUTE_SHADER) });

			std::shared_ptr<Image> image = Image::fromFile(file, 3);
			if (!image)
				return et;

			Texture envTextureEquirect = Renderer::createTexture(image, GL_RGB, GL_RGB16F, 1);

			glUseProgram(equirectToCubeProgram);
			glBindTextureUnit(0, envTextureEquirect.id);
			glBindImageTexture(0, envTextureUnfiltered.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute(envTextureUnfiltered.width / 32, envTextureUnfiltered.height / 32, 6);

			glDeleteTextures(1, &envTextureEquirect.id);
			glDeleteProgram(equirectToCubeProgram);
		}

		glGenerateTextureMipmap(envTextureUnfiltered.id);

		// Compute pre-filtered specular environment map.
		{
			GLuint spmapProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/spmap_cs.glsl", GL_COMPUTE_SHADER) });

			et.m_envTexture = Renderer::createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);

			// Copy 0th mipmap level into destination environment map.
			glCopyImageSubData(envTextureUnfiltered.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
				et.m_envTexture.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
				et.m_envTexture.width, et.m_envTexture.height, 6);

			glUseProgram(spmapProgram);
			glBindTextureUnit(0, envTextureUnfiltered.id);

			// Pre-filter rest of the mip chain.
			const float deltaRoughness = 1.0f / glm::max(float(et.m_envTexture.levels - 1), 1.0f);
			for (int level = 1, size = kEnvMapSize / 2; level <= et.m_envTexture.levels; ++level, size /= 2) {
				const GLuint numGroups = glm::max(1, size / 32);
				glBindImageTexture(0, et.m_envTexture.id, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glProgramUniform1f(spmapProgram, 0, level * deltaRoughness);
				glDispatchCompute(numGroups, numGroups, 6);
			}
			glDeleteProgram(spmapProgram);
		}

		glDeleteTextures(1, &envTextureUnfiltered.id);

		// Compute diffuse irradiance cubemap.
		{
			GLuint irmapProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/irmap_cs.glsl", GL_COMPUTE_SHADER) });

			et.m_irmapTexture = Renderer::createTexture(GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, GL_RGBA16F, 1);

			glUseProgram(irmapProgram);
			glBindTextureUnit(0, et.m_envTexture.id);
			glBindImageTexture(0, et.m_irmapTexture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glDispatchCompute(et.m_irmapTexture.width / 32, et.m_irmapTexture.height / 32, 6);
			glDeleteProgram(irmapProgram);
		}

		// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
		{
			GLuint spBRDFProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/spbrdf_cs.glsl", GL_COMPUTE_SHADER) });

			et.m_spBRDF_LUT = Renderer::createTexture(GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG16F, 1);
			glTextureParameteri(et.m_spBRDF_LUT.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(et.m_spBRDF_LUT.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glUseProgram(spBRDFProgram);
			glBindImageTexture(0, et.m_spBRDF_LUT.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
			glDispatchCompute(et.m_spBRDF_LUT.width / 32, et.m_spBRDF_LUT.height / 32, 1);
			glDeleteProgram(spBRDFProgram);
		}

		glFinish();
		et.m_valid = true;
		return et;
	}
}