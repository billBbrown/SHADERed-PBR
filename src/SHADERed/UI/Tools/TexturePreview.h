#pragma once
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ed {
	class TexturePreview {
	public:
		~TexturePreview();

		void Init(int w, int h);
		void Draw(GLuint tex);
		inline GLuint GetTexture() { return m_backTex; }

	private:
		float m_w, m_h;

		GLuint m_backShader;
		GLuint m_backFBO, m_backTex, m_backDepth;
		GLuint m_fsVAO, m_fsVBO;
		GLuint m_uMatWVPLoc;
	};
}