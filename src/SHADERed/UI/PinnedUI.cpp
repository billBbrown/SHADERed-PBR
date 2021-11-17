#include <SHADERed/UI/CodeEditorUI.h>
#include <SHADERed/UI/PinnedUI.h>
#include <imgui/imgui.h>

#include <SHADERed/Objects/Logger.h>

namespace ed {
	void PinnedUI::OnEvent(const SDL_Event& e)
	{
	}
	void PinnedUI::Update(float delta)
	{
		if (ImGui::Button("Sort by name")) {
			m_data->Parser.ModifyProject();
			std::sort(m_pinnedVars.begin(), m_pinnedVars.end(), [](const ed::ShaderVariable* a, const ed::ShaderVariable* b) -> bool {
				if (a->System == SystemShaderVariable::None) {
					if (b->System == SystemShaderVariable::None)
						return strcmp(a->Name, b->Name) < 0;
					else
						return false;
				} else {
					if (b->System == SystemShaderVariable::None)
						return true;
					else //Both system , compare name
						return strcmp(a->Name, b->Name) < 0;
				}
				return false;
			});
		}
		ImGui::NewLine();

		for (int i = 0; i < m_pinnedVars.size(); i++) { 
			ShaderVariable* var = m_pinnedVars[i];
			ImGui::PushID(i);

			m_editor.Open(var);
			bool doClose = false;
			if (m_editor.Update(&doClose))
				m_data->Parser.ModifyProject();

			/*ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - Settings::Instance().CalculateSize(25));
			if (ImGui::Button("CLOSE", ImVec2(Settings::Instance().CalculateSize(50), 0))) {
				Remove(var->Name);
				m_data->Parser.ModifyProject();
			}*/

			if (doClose) { //The upper button take too much space, changed to a small X close button
				Remove(var->Name);
				m_data->Parser.ModifyProject();
			}


			ImGui::NewLine();
			ImGui::Separator();
			ImGui::NewLine();

			ImGui::PopID();
		}

		if (m_pinnedVars.size() == 0)
			ImGui::TextWrapped("Pin variables here for easy access");
	}
	void PinnedUI::Add(ed::ShaderVariable* inp)
	{
		if (Contains(inp->Name)) {
			Logger::Get().Log("Cannot pin variable " + std::string(inp->Name) + " - it is already pinned", true);
			return;
		}

		Logger::Get().Log("Pinning variable " + std::string(inp->Name));

		m_data->Parser.ModifyProject();
		m_pinnedVars.push_back(inp);
	}
	void PinnedUI::Remove(const char* name)
	{
		for (int i = 0; i < m_pinnedVars.size(); i++) {
			if (strcmp(name, m_pinnedVars[i]->Name) == 0) {
				Logger::Get().Log("Unpinned variable " + std::string(name), true);
				m_pinnedVars.erase(m_pinnedVars.begin() + i);
				m_data->Parser.ModifyProject();
				return;
			}
		}

		Logger::Get().Log("Failed to unpin variable " + std::string(name) + " - not pinned", true);
	}
	bool PinnedUI::Contains(const char* data)
	{
		for (auto& var : m_pinnedVars)
			if (strcmp(var->Name, data) == 0)
				return true;
		return false;
	}

	void PinnedUI::SwitchPinnedVariableTargetToThis(std::vector<ed::ShaderVariable*>& els)
	{
		bool changed = false;
		for (size_t i = 0; i < m_pinnedVars.size(); ++i)
		{
			ed::ShaderVariable* var = m_pinnedVars[i];
			for (ed::ShaderVariable* outerVar : els)
			{
				if (var == outerVar) //No need to change
					continue;

				if (strcmp(var->Name, outerVar->Name) == 0 && var->GetType() == outerVar->GetType()) {
					m_pinnedVars[i] = outerVar;
					changed = true;
				}
			}
		}

		if (changed)
			m_data->Parser.ModifyProject();
	}

}