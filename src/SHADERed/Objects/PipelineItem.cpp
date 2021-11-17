#include "PipelineItem.h"

namespace ed {
	namespace pipe {
		void ApplyRenderStateToGL(const RenderState& stateIn)
		{
			const pipe::RenderState* state = &stateIn;
			//copy from RenderEngine

			// depth clamp
			if (state->DepthClamp)
				glEnable(GL_DEPTH_CLAMP);
			else
				glDisable(GL_DEPTH_CLAMP);

			// fill mode
			glPolygonMode(GL_FRONT_AND_BACK, state->PolygonMode);

			// culling and front face
			if (state->CullFace)
				glEnable(GL_CULL_FACE);
			else
				glDisable(GL_CULL_FACE);
			glCullFace(state->CullFaceType);
			glFrontFace(state->FrontFace);

			// disable blending
			if (state->Blend) {
				glEnable(GL_BLEND);
				glBlendEquationSeparate(state->BlendFunctionColor, state->BlendFunctionAlpha);
				glBlendFuncSeparate(state->BlendSourceFactorRGB, state->BlendDestinationFactorRGB, state->BlendSourceFactorAlpha, state->BlendDestinationFactorAlpha);
				glBlendColor(state->BlendFactor.r, state->BlendFactor.g, state->BlendFactor.a, state->BlendFactor.a);
				glSampleCoverage(state->AlphaToCoverage, GL_FALSE);
			} else
				glDisable(GL_BLEND);

			// depth state
			if (state->DepthTest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			glDepthMask(state->DepthMask);
			glDepthFunc(state->DepthFunction);
			glPolygonOffset(0.0f, state->DepthBias);

			// stencil
			if (state->StencilTest) {
				glEnable(GL_STENCIL_TEST);
				glStencilFuncSeparate(GL_FRONT, state->StencilFrontFaceFunction, 1, state->StencilReference);
				glStencilFuncSeparate(GL_BACK, state->StencilBackFaceFunction, 1, state->StencilReference);
				glStencilMask(state->StencilMask);
				glStencilOpSeparate(GL_FRONT, state->StencilFrontFaceOpStencilFail, state->StencilFrontFaceOpDepthFail, state->StencilFrontFaceOpPass);
				glStencilOpSeparate(GL_BACK, state->StencilBackFaceOpStencilFail, state->StencilBackFaceOpDepthFail, state->StencilBackFaceOpPass);
			} else
				glDisable(GL_STENCIL_TEST);
		}

		std::vector<ShaderVariable*>& GetShaderVariables(PipelineItem* pipelineItem)
		{
			if (pipelineItem != nullptr && pipelineItem->Data != nullptr) {
				bool isCompute = pipelineItem->Type == PipelineItem::ItemType::ComputePass;
				bool isAudio = pipelineItem->Type == PipelineItem::ItemType::AudioPass;
				void* itemData = pipelineItem->Data;
				return isCompute ? ((pipe::ComputePass*)itemData)->Variables.GetVariables() :
									 (isAudio ? ((pipe::AudioPass*)itemData)->Variables.GetVariables() :
												((pipe::ShaderPass*)itemData)->Variables.GetVariables());
			}
			static std::vector<ShaderVariable*> emptyVariables;
			return emptyVariables;
		}

	}
}