#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <SHADERed/Engine/AudioPlayer.h>

#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	
	namespace TextureHelper {
		struct TextureDesc;//Just declare
	}

	enum class ObjectType {
		Unknown,
		RenderTexture,
		CubeMap,
		Audio,
		Buffer,
		Image,
		Texture,
		Image3D,
		KeyboardTexture,
		PluginObject,
		Texture3D
	};

	/* object information */
	struct RenderTextureObject {
		GLuint DepthStencilBuffer, DepthStencilBufferMS, BufferMS; // ColorBuffer is stored in ObjectManager
		glm::ivec2 FixedSize;
		glm::vec2 RatioSize;
		glm::vec4 ClearColor;
		bool Clear;
		GLuint Format;

		RenderTextureObject()
				: FixedSize(-1, -1)
				, RatioSize(1, 1)
				, Clear(true)
				, ClearColor(0, 0, 0, 1)
				, Format(GL_RGBA)
		{
		}

		glm::ivec2 CalculateSize(int w, int h)
		{
			glm::ivec2 rtSize = FixedSize;
			if (rtSize.x == -1) {
				rtSize.x = RatioSize.x * w;
				rtSize.y = RatioSize.y * h;
			}

			return rtSize;
		}
	};
	struct BufferObject {
		int Size;
		void* Data;
		char ViewFormat[256]; // vec3;vec3;vec2
		bool PreviewPaused;
		GLuint ID;
	};
	struct ImageObject {
		glm::ivec2 Size;
		GLuint Format;
		char DataPath[SHADERED_MAX_PATH];
	};
	struct Image3DObject {
		glm::ivec3 Size;
		GLuint Format;
	};
	struct PluginObject {
		char Type[128];
		IPlugin1* Owner;

		GLuint ID;
		void* Data;
	};

	enum EnvironmentType : uint8_t {
		EnvironmentType_None = 0,
		EnvironmentType_Main = 1, //Not used
		EnvironmentType_Specular = 2, //Cube map for IBL specular 
		EnvironmentType_Iradiance = 3,	//Cube map for IBL irradiance
		EnvironmentType_BrdfLut = 4,		//Lut map for IBL specular brdf
		EnvironmentType_Origin = 5, //Origin Lat-long texture for inspection or other further usage
		EnvironmentType_Count,
	};

	/* object - TODO: maybe inheritance? class ImageObject : public ObjectManagerItem -> though, too many changes */
	class ObjectManagerItem {
	public:
		ObjectManagerItem(const std::string& name, ObjectType type);
		~ObjectManagerItem();

		std::string Name;
		ObjectType Type;

		glm::ivec2 TextureSize;
		int Depth;
		std::unique_ptr<TextureHelper::TextureDesc> TextureDetail; //Save more detail
		GLuint Texture, FlippedTexture;
		std::vector<std::string> CubemapPaths;
		
		EnvironmentType EnvironmentTypeValue;

		bool Texture_VFlipped;
		GLuint Texture_MinFilter, Texture_MagFilter, Texture_WrapS, Texture_WrapT, Texture_WrapR;

		eng::AudioPlayer* Sound;
		bool SoundMuted;

		RenderTextureObject* RT;
		BufferObject* Buffer;
		ImageObject* Image;
		Image3DObject* Image3D;

		PluginObject* Plugin;
	};
}