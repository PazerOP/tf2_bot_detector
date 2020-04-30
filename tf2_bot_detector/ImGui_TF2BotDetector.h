#pragma once

#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>

#include <string_view>

namespace ImGui
{
	void TextUnformatted(const std::string_view& text);
	void TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end = nullptr);
	void TextColoredUnformatted(const ImVec4& col, const std::string_view& text);
}
