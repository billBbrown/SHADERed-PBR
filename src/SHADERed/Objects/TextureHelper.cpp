/*
* Physically Based Rendering
* Copyright (c) 2017-2018 Michał Siejak
*
* OpenGL 4.5 renderer.
*/

#include "TextureHelper.h"
#include <SHADERed/UI/Tools/TexturePreview.h>
#include "image.hpp"

#include <GLFW/glfw3.h>
#include <gl/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "stb_image.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <iostream>
#include <map>
#include "Logger.h"

namespace ed {
	namespace TextureHelper {
		static constexpr int kEnvMapSize = 1024;
		static constexpr int kIrradianceMapSize = 32;
		static constexpr int kBRDF_LUT_Size = 256;

		//////////////////////////////////////////////////////////////////////////

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

		//////////////////////////////////////////////////////////////////////////

		class Renderer {
		public:
			struct Capacity {
				float maxAnisotropy = 1.0f;
			};

			static Capacity m_capabilities;

			static Texture createTextureInternal(GLenum target, int width, int height, GLenum format, GLenum type, GLenum internalformat, int levels = 0)
			{
				Texture texture;
				texture.target = target;
				texture.width = width;
				texture.height = height;
				texture.levels = (levels > 0) ? levels : Utility::numMipmapLevels(width, height);
				texture.format = format;
				texture.type = type;
				texture.internalFormat = internalformat;
				assert(texture.Validate());//Make sure the format is valid

				glCreateTextures(target, 1, &texture.id);
				glTextureStorage2D(texture.id, texture.levels, internalformat, width, height);
				glTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER, texture.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
				glTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTextureParameterf(texture.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_capabilities.maxAnisotropy);
				return texture;
			}
			static Texture createTexture(const std::shared_ptr<class Image>& image, GLenum format, GLenum type, GLenum internalformat, int levels = 0)
			{
				Texture texture = createTextureInternal(
					GL_TEXTURE_2D, image->width(), image->height(), format, type, internalformat, levels);
				if (image->isHDR()) {
					assert(type == GL_FLOAT);
					glTextureSubImage2D(
						texture.id, 0, 0, 0, texture.width, texture.height, format, GL_FLOAT, image->pixels<float>());
				} else {
					assert(type == GL_UNSIGNED_BYTE);
					glTextureSubImage2D(
						texture.id, 0, 0, 0, texture.width, texture.height, format, GL_UNSIGNED_BYTE, image->pixels<unsigned char>());
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

		//////////////////////////////////////////////////////////////////////////
		std::map<GLenum, int> GFormatToChannel {
			{ GL_RED, 1 },
			{ GL_GREEN, 1 },
			{ GL_BLUE, 1 },
			{ GL_RG, 2 },
			{ GL_RGB, 3 },
			{ GL_RGBA, 4 },
		};
		int Texture::GetChannelCount() const
		{
			assert(GFormatToChannel.find(format) != GFormatToChannel.end());
			int channelCount = GFormatToChannel[format];
			return channelCount;
		}

		int Texture::GetChannelCount(GLenum format)
		{
			assert(GFormatToChannel.find(format) != GFormatToChannel.end());
			int channelCount = GFormatToChannel[format];
			return channelCount;
		}

		std::map<GLenum, int> GTypeToChannelSize {
			{ GL_UNSIGNED_BYTE, 1 },
			{ GL_BYTE, 1 },
			{ GL_FLOAT, 4 }, //GL_FLOAT means 32bit float
			{ GL_HALF_FLOAT, 2 },
			//other wired type like SHORT_4_4_4_4 is not supported
		};

		int Texture::GetChannelSize()const
		{
			assert(GTypeToChannelSize.find(type) != GTypeToChannelSize.end());
			return GTypeToChannelSize[type];
		}

		int Texture::GetChannelSize(GLenum type)
		{
			assert(GTypeToChannelSize.find(type) != GTypeToChannelSize.end());
			return GTypeToChannelSize[type];
		}

		bool Texture::IsFloatPixel() const
		{
			return type == GL_FLOAT || type == GL_HALF_FLOAT;
		}

		bool Texture::IsFloatPixel(GLenum type)
		{
			return type == GL_FLOAT || type == GL_HALF_FLOAT;
		}
		//////////////////////////////////////////////////////////////////////////

		bool SaveCubeTextureToFile(Texture cubeTexture, const std::string& fileName, SavedTexturePathResult* savedResult) // this save all 6 faces of cube map into images in harddrive
		{
//https://community.khronos.org/t/output-cubemap-texture-object-as-bitmap-images/75445
//Alter a lot
#if 1
			int channelCount = cubeTexture.GetChannelCount();
			int pixelSize = cubeTexture.GetChannelSize() * channelCount;

			GLsizei width = cubeTexture.width;
			GLsizei height = cubeTexture.height;
			assert(cubeTexture.width == cubeTexture.height);

			size_t bufferSize = pixelSize * cubeTexture.width * cubeTexture.height;

			std::vector<uint8_t> posZBuffer(bufferSize), negZBuffer(bufferSize), posYBuffer(bufferSize), negYBuffer(bufferSize), negXBuffer(bufferSize), posXBuffer(bufferSize);

			// Create a framebuffer
			GLuint CarrierText;
			GLuint CarrierFBo;

			glGenTextures(1, &CarrierText);
			glBindTexture(GL_TEXTURE_CUBE_MAP, CarrierText);

			glGenFramebuffers(1, &CarrierFBo);
			glBindFramebuffer(GL_FRAMEBUFFER, CarrierFBo);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, cubeTexture.format, cubeTexture.type, &posXBuffer[0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, cubeTexture.format, cubeTexture.type, &negXBuffer[0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, cubeTexture.format, cubeTexture.type, &posYBuffer[0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, cubeTexture.format, cubeTexture.type, &negYBuffer[0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, cubeTexture.format, cubeTexture.type, &posZBuffer[0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, cubeTexture.id, 0);
			glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, cubeTexture.format, 0, 0, width, height, 0);
			glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, cubeTexture.format, cubeTexture.type, &negZBuffer[0]);

			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &CarrierFBo);
			glDeleteTextures(1, &CarrierText);

			GLenum err;
			while ((err = glGetError()) != GL_NO_ERROR) {
				std::cerr << "OpenGL error: " << gluErrorString(err) << std::endl;
			}

			auto originName = std::filesystem::path(fileName);
			auto dir = originName.parent_path();
			auto fileNameStem = originName.stem();
			auto extension = originName.extension().string();
			std::string baseFileName = (dir / fileNameStem).string();

			auto posXPath = baseFileName + ".posX" + extension;
			if (!Image::writeFile(posXPath, width, height, channelCount, pixelSize * width, &posXBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[0] = posXPath;

			auto negXPath = baseFileName + ".negX" + extension;
			if (!Image::writeFile(negXPath, width, height, channelCount, pixelSize * width, &negXBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[1] = (negXPath);

			auto posYPath = baseFileName + ".posY" + extension;
			if (!Image::writeFile(posYPath, width, height, channelCount, pixelSize * width, &posYBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[2] = (posYPath);

			auto negYPath = baseFileName + ".negY" + extension;
			if (!Image::writeFile(negYPath, width, height, channelCount, pixelSize * width, &negYBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[3] = (negYPath);

			auto posZPath = baseFileName + ".posZ" + extension;
			if (!Image::writeFile(posZPath, width, height, channelCount, pixelSize * width, &posZBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[4] =(posZPath);

			auto negZPath = baseFileName + ".negZ" + extension;
			if (!Image::writeFile(negZPath, width, height, channelCount, pixelSize * width, &negZBuffer[0]))
				return false;
			if (savedResult != nullptr)
				savedResult->SavedPath[5] = (negZPath);

#endif
			return true;
		}

		bool Save2DTextureToFile(Texture textureInput, const std::string& fileName)
		{

			int channelCountPretest = textureInput.GetChannelCount();
			#if 0 //I used to use the following code ,but found that Texture2DPreview is much better a choise
			//https://stackoverflow.com/questions/56140002/saving-a-glteximage2d-to-the-file-system-for-inspection
			//painful~~~~			
			if (channelCountPretest != 2) //rg alike texture, can't be save like this, will cause error
			{
				int channelCount = channelCountPretest;
				int pixelSize = textureInput.GetChannelSize() * channelCount;
				size_t bufferSize = pixelSize * textureInput.width * textureInput.width;
				std::vector<uint8_t> buffer(bufferSize);

				GLuint fbo;
				glGenFramebuffers(1, &fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInput.id, 0);

				glReadPixels(0, 0, textureInput.width, textureInput.height, textureInput.format, textureInput.type, &buffer[0]);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDeleteFramebuffers(1, &fbo);

				return Image::writeFile(fileName, textureInput.width, textureInput.height, channelCount,
					pixelSize * textureInput.width, &buffer[0]);
			} else 
			#endif
			// 
			{
				//First draw to rgb/rgba
				GLenum previewType = textureInput.IsFloatPixel() ? GL_FLOAT : GL_UNSIGNED_BYTE;
				GLenum previewFormat = textureInput.HasAlpha()? GL_RGBA : GL_RGB;
				GLuint internalFormat = textureInput.HasAlpha() ?
					  (textureInput.IsFloatPixel() ? GL_RGBA32F : GL_RGBA8) :
					  (textureInput.IsFloatPixel() ? GL_RGB32F : GL_RGB8);

				if (textureInput.IsFloatPixel()) {
					if (fileName.rfind(".hdr") == -1) //Must use .hdr to store float
						return false;
				}

				ed::TexturePreview preview;
				preview.Init(textureInput.width, textureInput.height, 
					internalFormat, 
					previewFormat, 
					previewType);
				preview.Draw(textureInput.id);

				//then get the value
				int channelCount = Texture::GetChannelCount(previewFormat);
				int pixelSize = Texture::GetChannelSize(previewType) * channelCount;

				size_t bufferSize = pixelSize * textureInput.width * textureInput.width;
				std::vector<uint8_t> buffer(bufferSize);

				glReadPixels(0, 0, textureInput.width, textureInput.height, previewFormat, previewType, &buffer[0]);

				/*GLuint fbo;
				glGenFramebuffers(1, &fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, preview.GetTexture(), 0);

				glReadPixels(0, 0, textureInput.width, textureInput.height, previewFormat, previewType, &buffer[0]);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDeleteFramebuffers(1, &fbo);*/

				return Image::writeFile(fileName, textureInput.width, textureInput.height, channelCount,
					pixelSize * textureInput.width, &buffer[0]);
			}
		}

		bool SaveTextureToFile(Texture textureInput, const std::string& fileName, SavedTexturePathResult* savedResult) // this save all 6 faces of cube map into images in harddrive
		{
			std::string fileNameUsing = fileName;
			if (textureInput.IsFloatPixel()) {
				auto pth = std::filesystem::path(fileNameUsing);
				auto dir = pth.parent_path();
				auto stem = pth.stem();
				fileNameUsing = (dir / stem).string() + ".hdr";
			}

			if (textureInput.target == GL_TEXTURE_CUBE_MAP) {
				if (!SaveCubeTextureToFile(textureInput, fileNameUsing, savedResult))
					return false;
			} else if (textureInput.target == GL_TEXTURE_2D) {
				if (!Save2DTextureToFile(textureInput, fileNameUsing))
					return false;
				if (savedResult != nullptr)
					savedResult->SavedPath[0] = fileNameUsing;
			}
			if (savedResult) {
				savedResult->SavedTextureTarget = textureInput.target;
			}
			return true; //GL_TEXTURE_3D is not yet implemented
		}

		EnvironmentTexture Create(const std::string& file)
		{
			const GLenum cubeFormat = GL_RGBA; //May change some day
			const GLenum cubeType = GL_FLOAT;
			const GLenum cubInternalFormat = GL_RGBA32F; //Make it higher

			EnvironmentTexture et;
			//Global
			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
			// Unfiltered environment cube map (temporary).
			Texture envTextureUnfiltered = Renderer::createTextureInternal(
				GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, cubeFormat, cubeType, cubInternalFormat);

			// Load & convert equirectangular environment map to a cubemap texture.
			{
				GLuint equirectToCubeProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/equirect2cube_cs.glsl", GL_COMPUTE_SHADER) });

				std::shared_ptr<Image> image = Image::fromFile(file, 3);
				if (!image) {
					Logger::Get().Log("Image::fromFile failed for: " + file, true);
					return et;
				}

				// The input use a specific format

				Texture envTextureEquirect = Renderer::createTexture(image, GL_RGB,
					image->isHDR() ? GL_FLOAT : GL_UNSIGNED_BYTE,
					GL_RGB16F, 1);
				et.m_originTexture = envTextureEquirect;

				glUseProgram(equirectToCubeProgram);
				glBindTextureUnit(0, envTextureEquirect.id);
				glBindImageTexture(0, envTextureUnfiltered.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, cubInternalFormat);
				glDispatchCompute(envTextureUnfiltered.width / 32, envTextureUnfiltered.height / 32, 6);

				//glDeleteTextures(1, &envTextureEquirect.id);
				glDeleteProgram(equirectToCubeProgram);
			}

			glGenerateTextureMipmap(envTextureUnfiltered.id);

			// Compute pre-filtered specular environment map.
			{
				GLuint spmapProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/spmap_cs.glsl", GL_COMPUTE_SHADER) });

				et.m_envTexture = Renderer::createTextureInternal(
					GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, cubeFormat, cubeType, cubInternalFormat);

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
					glBindImageTexture(0, et.m_envTexture.id, level, GL_TRUE, 0, GL_WRITE_ONLY, cubInternalFormat);
					glProgramUniform1f(spmapProgram, 0, level * deltaRoughness);
					glDispatchCompute(numGroups, numGroups, 6);
				}
				glDeleteProgram(spmapProgram);
			}

			glDeleteTextures(1, &envTextureUnfiltered.id);

			// Compute diffuse irradiance cubemap.
			{
				GLuint irmapProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/irmap_cs.glsl", GL_COMPUTE_SHADER) });

				et.m_irmapTexture = Renderer::createTextureInternal(
					GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, cubeFormat, cubeType, cubInternalFormat, 1);

				glUseProgram(irmapProgram);
				glBindTextureUnit(0, et.m_envTexture.id);
				glBindImageTexture(0, et.m_irmapTexture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, cubInternalFormat);
				glDispatchCompute(et.m_irmapTexture.width / 32, et.m_irmapTexture.height / 32, 6);
				glDeleteProgram(irmapProgram);
			}

			// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
			{
				GLuint spBRDFProgram = Renderer::linkProgram({ Renderer::compileShader("data/shaders/glsl/spbrdf_cs.glsl", GL_COMPUTE_SHADER) });

				et.m_spBRDF_LUT = Renderer::createTextureInternal(GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG, GL_FLOAT, GL_RG16F, 1);
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

	void stbi_vertical_flip(void* image, int w, int h, int bytes_per_pixel) //copy from stbi__vertical_flip
	{
		int row;
		size_t bytes_per_row = (size_t)w * bytes_per_pixel;
		stbi_uc temp[2048];
		stbi_uc* bytes = (stbi_uc*)image;

		for (row = 0; row < (h >> 1); row++) {
			stbi_uc* row0 = bytes + row * bytes_per_row;
			stbi_uc* row1 = bytes + (h - row - 1) * bytes_per_row;
			// swap row0 with row1
			size_t bytes_left = bytes_per_row;
			while (bytes_left) {
				size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
				memcpy(temp, row0, bytes_copy);
				memcpy(row0, row1, bytes_copy);
				memcpy(row1, temp, bytes_copy);
				row0 += bytes_copy;
				row1 += bytes_copy;
				bytes_left -= bytes_copy;
			}
		}
	}
}