#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/PluginAPI/Plugin.h>
#include <SHADERed/Objects/TextureHelper.h>
#include "ObjectManagerItem.h"

ed::ObjectManagerItem::ObjectManagerItem(const std::string& name, ObjectType type)
{
	Name = name;
	Type = type;

	TextureSize = glm::ivec2(0, 0);
	Depth = 1;
	Texture = 0;
	FlippedTexture = 0;
	Texture_VFlipped = false;
	Texture_MinFilter = GL_LINEAR;
	Texture_MagFilter = GL_NEAREST;
	Texture_WrapS = GL_REPEAT;
	Texture_WrapT = GL_REPEAT;
	Texture_WrapR = GL_REPEAT;
	CubemapPaths.clear();
	Sound = nullptr;
	SoundMuted = false;
	RT = nullptr;
	Buffer = nullptr;
	Image = nullptr;
	Image3D = nullptr;
	Plugin = nullptr;
	EnvironmentTypeValue = EnvironmentType_None;
}

ed::ObjectManagerItem::~ObjectManagerItem()
{
	if (Buffer != nullptr) {
		free(Buffer->Data);
		delete Buffer;
	}
	if (Image != nullptr)
		delete Image;
	if (Image3D != nullptr)
		delete Image3D;

	if (RT != nullptr) {
		glDeleteTextures(1, &RT->DepthStencilBuffer);
		delete RT;
	}
	if (Sound != nullptr)
		delete Sound;
	if (Plugin != nullptr)
		delete Plugin;

	glDeleteTextures(1, &Texture);
	glDeleteTextures(1, &FlippedTexture);
	CubemapPaths.clear();
}
