#pragma once
#include <gl/glew.h>
#include <string>

namespace ed {

	void stbi_vertical_flip(void* image, int w, int h, int bytes_per_pixel); //copy from stbi__vertical_flip

	enum CubeFace {
		//Really confusing, need enum name to help
		CubeFace_Left = 0,
		CubeFace_NegativeX = 0,

		CubeFace_Top = 1,
		CubeFace_PositiveY = 1,

		CubeFace_Front = 2,
		CubeFace_NegativeZ = 2,

		CubeFace_Bottom = 3,
		CubeFace_NegativeY = 3,

		CubeFace_Right = 4,
		CubeFace_PositiveX = 4,

		CubeFace_Back = 5,
		CubeFace_PositiveZ = 5,

		Count = 6,
	};

	namespace TextureHelper {

		struct TextureDesc {
			GLenum target = GL_TEXTURE_2D;
			GLuint id = 0;
			int width = 0, height = 0;
			int levels = 1;
			GLenum format = GL_RGB;
			GLenum type = GL_UNSIGNED_BYTE;
			GLenum internalFormat = GL_RGB8;
			int GetChannelSize() const;
			int GetChannelCount() const;
			bool IsFloatPixel()const;
			bool Validate() const { return id != 0 && GetChannelCount() > 0 && GetChannelSize() > 0; }
			bool HasAlpha() const { return GetChannelCount() == 4; }

			static int GetChannelSize(GLenum type);
			static int GetChannelCount(GLenum format);
			static bool IsFloatPixel(GLenum type);

			void Destroy() {
				GLuint idToDestroy = this->id;
				glDeleteTextures(1, &idToDestroy);

				this->id = 0;  //Make it invalid
			}
		};

		struct EnvironmentTexture {
			TextureDesc m_originTexture;
			TextureDesc m_envTexture;
			TextureDesc m_irmapTexture;
			TextureDesc m_spBRDF_LUT;
			bool m_valid = false;

			void Destroy() {
				m_originTexture.Destroy();
				m_envTexture.Destroy();
				m_irmapTexture.Destroy();
				m_spBRDF_LUT.Destroy();
				m_valid = false;
			}
		};

		//////////////////////////////////////////////////////////////////////////

		class Utility {
		public:
			template <typename T>
			inline static constexpr bool isPowerOfTwo(T value)
			{
				return value != 0 && (value & (value - 1)) == 0;
			}
			template <typename T>
			inline static constexpr T roundToPowerOfTwo(T value, int POT)
			{
				return (value + POT - 1) & -POT;
			}
			template <typename T>
			inline static constexpr T numMipmapLevels(T width, T height)
			{
				T levels = 1;
				while ((width | height) >> levels) {
					++levels;
				}
				return levels;
			}
		};

		//////////////////////////////////////////////////////////////////////////

		EnvironmentTexture Create(const std::string& file);

		struct SavedTexturePathResult {
			enum {
				MAX_POSIBLE_TEXTURE = 6, //Happen on cubemap, save 1, become 6, since this engine use 6 face format, it may be a better choice to save it separately instead of saving a single dds or cross-cubemap
			};
			GLenum SavedTextureTarget; //GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D, GL_TEXTURE_3D
			std::string SavedPath[MAX_POSIBLE_TEXTURE];
		};

		bool SaveTextureToFile(TextureDesc cubeTexture, const std::string& fileName, 
			bool flipYBeforeSave = true, SavedTexturePathResult* savedResult = nullptr); //save the texture to file ; if its a float texture, extension will be forcely changed to .hdr;
	}

	
}