#include "ImGui_TF2BotDetector.h"

void ImGui::TextUnformatted(const std::string_view& text)
{
	return ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void ImGui::TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end)
{
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	ImGui::TextUnformatted(text, text_end);
	ImGui::PopStyleColor();
}

void ImGui::TextColoredUnformatted(const ImVec4& col, const std::string_view& text)
{
	ImGui::TextColoredUnformatted(col, text.data(), text.data() + text.size());
}
