#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Engine/GeometryFactory.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/UI/Tools/TexturePreview.h>
#include <SHADERed/Objects/TextureHelper.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const char* TEXTUREMAP_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iUV;

uniform mat4 uMatWVP;
out vec2 uv;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
	uv = iUV;
}
)";


const char* TEXTUREMAP_PS_CODE = R"(
#version 330

uniform sampler2D textureMap;

in vec2 uv;
out vec4 fragColor;

void main()
{
    vec2 localUV = uv;

    fragColor = clamp(textureLod(textureMap, localUV, 0), vec4(0.0f), vec4(1.0f));
}
)";

namespace ed {
	TexturePreview::~TexturePreview()
	{
		glDeleteBuffers(1, &m_fsVBO);
		glDeleteVertexArrays(1, &m_fsVAO);
		glDeleteTextures(1, &m_backTex);
		glDeleteTextures(1, &m_backDepth);
		glDeleteFramebuffers(1, &m_backFBO);
	}
	void TexturePreview::Init(int w, int h, GLuint internalFormat /*= GL_RGBA*/, GLenum format /*= GL_RGBA*/, GLenum type /*= GL_UNSIGNED_BYTE*/)
	{
		Logger::Get().Log("Setting up textureMap preview system...");

		m_w = w;
		m_h = h;
		//If change this , also need to change GetChannelCount()
		
		m_backShader = ed::gl::CreateShader(&TEXTUREMAP_VS_CODE, &TEXTUREMAP_PS_CODE, "textureMap projection");

		glUseProgram(m_backShader);
		m_uMatWVPLoc = glGetUniformLocation(m_backShader, "uMatWVP");
		glUniform1i(glGetUniformLocation(m_backShader, "textureMap"), 0);
		glUseProgram(0);

		m_fsVAO = ed::eng::GeometryFactory::CreatePlane(m_fsVBO, w, h, gl::CreateDefaultInputLayout());
		m_backFBO = gl::CreateSimpleFramebuffer(w, h, m_backTex, m_backDepth, 
			internalFormat, 
			format, 
			type);

		if (m_backFBO == 0)
			ed::Logger::Get().Log("Failed to create textureMap preview texture", true);
	}
	void TexturePreview::Draw(GLuint tex)
	{
		// bind fbo and buffers
		glBindFramebuffer(GL_FRAMEBUFFER, m_backFBO);
		static const GLuint fboBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, fboBuffers);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
		glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)));
		glViewport(0, 0, m_w, m_h);
		glUseProgram(m_backShader);

		glm::mat4 matVP = glm::ortho(0.0f, m_w, m_h, 0.0f);
		glm::mat4 matW = glm::translate(glm::mat4(1.0f), glm::vec3(m_w / 2, m_h / 2, 0.0f));
		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matVP * matW));

		// bind shader resource views
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);

		glBindVertexArray(m_fsVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	GLuint TexturePreview::DrawToGUITexture(GLuint tex, GLenum pixelType)
	{
		if (TextureHelper::TextureDesc::IsFloatPixel(pixelType)) {
			this->Draw(tex);
			return this->GetTexture();
		}
		return tex; //Normal texture don't need extra drawing, and that can preserve mipmap
	}

}