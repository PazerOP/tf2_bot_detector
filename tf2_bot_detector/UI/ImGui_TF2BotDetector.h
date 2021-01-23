#pragma once

#include "ReleaseChannel.h"

#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui_desktop/ScopeGuards.h>
#include <imgui.h>
#include <mh/raii/scope_exit.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/types/disable_copy_move.hpp>
#include <misc/cpp/imgui_stdlib.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <type_traits>

namespace ImGui
{
	template<typename... TArgs>
	inline auto TextFmt(const std::string_view& fmtStr, const TArgs&... args) ->
		decltype(mh::format(fmtStr, args...), void())
	{
		if constexpr (sizeof...(TArgs) > 0)
			return TextFmt(mh::fmtstr<3073>(fmtStr, args...));
		else
			return TextUnformatted(fmtStr.data(), fmtStr.data() + fmtStr.size());
	}
	template<typename... TArgs>
	inline auto TextFmt(const ImVec4& color, const std::string_view& fmtStr, const TArgs&... args) ->
		decltype(TextFmt(fmtStr, args...))
	{
		ImGuiDesktop::TextColor scope(color);
		TextFmt(fmtStr, args...);
	}

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

	void HorizontalScrollBox(const char* ID, void(*contentsFn)(void* userData), void* userData = nullptr);
	inline void HorizontalScrollBox(const char* ID, void(*contentsFn)(const void* userData), const void* userData = nullptr)
	{
		return HorizontalScrollBox(ID, reinterpret_cast<void(*)(void*)>(contentsFn), const_cast<void*>(userData));
	}
	template<typename TFunc>
	void HorizontalScrollBox(const char* ID, const TFunc& func)
	{
		return HorizontalScrollBox(ID, [](const void* userData)
			{
				(*reinterpret_cast<const TFunc*>(userData))();
			}, reinterpret_cast<const void*>(&func));
	}

	void TextRightAligned(const std::string_view& text, float offsetX = -1);
	void TextRightAlignedF(const char* fmt, ...) IM_FMTARGS(1);

	namespace detail
	{
		using PopupScope = decltype(mh::scope_exit(ImGui::EndPopup));
	}

	inline detail::PopupScope BeginPopupContextItemScope(const char* str_id = nullptr, ImGuiMouseButton mouse_button = 1)
	{
		return detail::PopupScope(ImGui::EndPopup, BeginPopupContextItem(str_id, mouse_button));
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

	template<typename TFunc>
	inline bool InputTextWithHint(const char* label, const char* hint, std::string* str,
		ImGuiInputTextFlags flags, TFunc&& callback)
	{
		const auto patch = [](ImGuiInputTextCallbackData* data) -> int
		{
			void* funcPtr = data->UserData;
			data->UserData = nullptr;
			return (*reinterpret_cast<std::remove_reference_t<TFunc>*>(funcPtr))(data);
		};

		return ImGui::InputTextWithHint(label, hint, str, flags, patch, const_cast<void*>(reinterpret_cast<const void*>(&callback)));
	}

	ImVec2 CalcButtonSize(const char* label);

	void Value(const char* prefix, double v, const char* float_format = nullptr);
	template<typename TPrefix, typename TValue>
	inline void Value(const TPrefix& prefix, const TValue& v)
	{
		using namespace std::string_view_literals;
		ImGui::TextFmt("{}: {}"sv, prefix, v);
	}

	template<typename... TArgs>
	inline bool SetHoverTooltip(const std::string_view& fmtStr, const TArgs&... args)
	{
		if (IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(500);

			ImGui::TextFmt(fmtStr, args...);

			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
			return true;
		}

		return false;
	}

	void PushDisabled();  // NOTE: Use EnabledSwitch instead
	void PopDisabled();   // NOTE: Use EnabledSwitch instead

	template<typename TFunc>
	inline void EnabledSwitch(bool enabled, TFunc&& func, const std::string_view& disabledTooltip = {})
	{
		if (enabled)
		{
			if constexpr (std::is_invocable_v<TFunc, bool>)
				func(true);
			else
				func();
		}
		else
		{
			ImGui::BeginGroup();
			{
				ImGuiDesktop::ScopeGuards::GlobalAlpha globalAlpha(0.65f);
				ImGui::PushDisabled();

				if constexpr (std::is_invocable_v<TFunc, bool>)
					func(false);
				else
					func();

				ImGui::PopDisabled();
			}
			ImGui::EndGroup();

			if (!disabledTooltip.empty())
			{
				ImGuiDesktop::ScopeGuards::GlobalAlpha tooltipAlpha(1);
				ImGui::SetHoverTooltip(disabledTooltip);
			}
		}
	}

	inline void SameLineNoPad() { ImGui::SameLine(0, 0); }

	void PacifierText();
}

namespace tf2_bot_detector
{
	class SteamID;

	bool InputTextSteamIDOverride(const char* label_id, SteamID& steamID, bool requireValid = true);
	bool InputTextTFDirOverride(const std::string_view& label_id, std::filesystem::path& path,
		const std::filesystem::path& autodetectedPath, bool requireValid = false);
	bool InputTextSteamDirOverride(const std::string_view& label_id, std::filesystem::path& path, bool requireValid = false);
	bool InputTextLocalIPOverride(const std::string_view& label_id, std::string& ip, bool requireValid = false);
	bool InputTextSteamAPIKey(const char* label_id, std::string& key, bool requireValid = false);
	bool Combo(const char* label_id, std::optional<ReleaseChannel>& mode);
	bool AutoLaunchTF2Checkbox(bool& value);
}

namespace ImPlot
{
	template<typename TFunc, typename = std::enable_if_t<std::is_invocable_v<TFunc, int>>>
	inline void PlotLine(const char* label_id, TFunc getter, int count, int offset = 0)
	{
		return ImPlot::PlotLine(label_id,
			[](void* data, int idx) { return (*reinterpret_cast<TFunc*>(data))(idx); },
			reinterpret_cast<void*>(&getter),
			count, offset);
	}
}
