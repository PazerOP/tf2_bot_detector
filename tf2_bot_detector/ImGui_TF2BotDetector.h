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

	void TextRightAligned(const std::string_view& text, float offsetX = -1);
	void TextRightAlignedF(const char* fmt, ...) IM_FMTARGS(1);

	namespace detail
	{
		struct DisableCopyMove
		{
			DisableCopyMove() = default;
			DisableCopyMove(const DisableCopyMove&) = delete;
			DisableCopyMove(DisableCopyMove&&) = delete;
			DisableCopyMove& operator=(const DisableCopyMove&) = delete;
			DisableCopyMove& operator=(DisableCopyMove&&) = delete;
		};

		struct [[nodiscard]] PopupScope final : DisableCopyMove
		{
			constexpr explicit PopupScope(bool enabled) : m_Enabled(enabled) {}
			~PopupScope() { if (m_Enabled) { ImGui::EndPopup(); } }
			constexpr operator bool() const { return m_Enabled; }
			bool m_Enabled;
		};
	}

	inline detail::PopupScope BeginPopupContextItemScope(const char* str_id = nullptr, ImGuiMouseButton mouse_button = 1)
	{
		return detail::PopupScope(BeginPopupContextItem(str_id, mouse_button));
	}

	inline void PlotLines(const char* label, float(*values_getter)(const void* data, int idx), const void* data, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0))
	{
		return PlotLines(label, reinterpret_cast<float(*)(void*, int)>(values_getter), const_cast<void*>(data),
			values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
	}

	template<typename TFunc>
	inline void PlotLines(const char* label, const TFunc& values_getter, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0))
	{
		return PlotLines(label,
			[](const void* data, int idx) { return static_cast<float>((*reinterpret_cast<const TFunc*>(data))(idx)); },
			reinterpret_cast<const void*>(&values_getter),
			values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
	}

	template<typename TFunc>
	inline void Combo(const char* label, int* current_item, TFunc items_getter, int items_count, int popup_max_height_in_items = -1)
	{
		const auto patch = [](void* data, int idx, const char** out_text)
		{
			return (*reinterpret_cast<TFunc*>(data))(idx, out_text);
		};

		return ImGui::Combo(label, current_item, patch, const_cast<void*>(&items_getter),
			items_count, popup_max_height_in_items);
	}
}
