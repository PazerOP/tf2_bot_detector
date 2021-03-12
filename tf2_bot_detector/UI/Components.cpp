#include "Components.h"
#include "UI/ImGui_TF2BotDetector.h"

bool tf2_bot_detector::UI::DrawColorPickers(const char* id, const std::initializer_list<ColorPicker>& pickers)
{
	bool modified = false;
	ImGui::HorizontalScrollBox(id, [&]
		{
			for (const auto& picker : pickers)
			{
				modified |= DrawColorPicker(picker.m_Name, picker.m_Color);
				ImGui::SameLine();
			}
		});
	return modified;
}

bool tf2_bot_detector::UI::DrawColorPicker(const char* name, std::array<float, 4>& color)
{
	return ImGui::ColorEdit4(name, color.data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview);
}
