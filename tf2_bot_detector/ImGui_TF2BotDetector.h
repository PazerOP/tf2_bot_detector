#pragma once

#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui.h>

#include <string_view>

namespace ImGui
{
	void TextUnformatted(const std::string_view& text);
	void TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end = nullptr);
	void TextColoredUnformatted(const ImVec4& col, const std::string_view& text);

	void AutoScrollBox(const char* ID, ImVec2 size, void(*contentsFn)(void* userData), void* userData = nullptr);
	inline void AutoScrollBox(const char* ID, ImVec2 size, void(*contentsFn)(const void* userData), const void* userData = nullptr)
	{
		return AutoScrollBox(ID, size, reinterpret_cast<void(*)(void*)>(contentsFn), const_cast<void*>(userData));
	}

	template<typename TFunc>
	void AutoScrollBox(const char* ID, ImVec2 size, const TFunc& func)
	{
		return AutoScrollBox(ID, size, [](const void* userData)
			{
				(*reinterpret_cast<const TFunc*>(userData))();
			}, reinterpret_cast<const void*>(&func));
	}
}
