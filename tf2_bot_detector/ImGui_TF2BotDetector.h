#pragma once

#include <imgui_desktop/ImGuiHelpers.h>
#include <imgui_desktop/ScopeGuards.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <type_traits>

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
	void Value(const char* prefix, const char* str);
	void Value(const char* prefix, uint64_t v);

	void SetHoverTooltip(const char* tooltipFmt, ...);

	template<typename TFunc>
	inline void EnabledSwitch(bool enabled, TFunc&& func, const char* disabledTooltip = nullptr)
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
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);


				if constexpr (std::is_invocable_v<TFunc, bool>)
					func(false);
				else
					func();

				ImGui::PopItemFlag();
			}
			ImGui::EndGroup();

			if (disabledTooltip)
			{
				ImGuiDesktop::ScopeGuards::GlobalAlpha tooltipAlpha(1);
				ImGui::SetHoverTooltip("%s", disabledTooltip);
			}
		}
	}
}

namespace tf2_bot_detector
{
	enum class ProgramUpdateCheckMode;
	class SteamID;

	bool InputTextSteamIDOverride(const char* label_id, SteamID& steamID, bool requireValid = true);
	bool InputTextTFDirOverride(const std::string_view& label_id, std::filesystem::path& path,
		const std::filesystem::path& autodetectedPath, bool requireValid = false);
	bool InputTextSteamDirOverride(const std::string_view& label_id, std::filesystem::path& path, bool requireValid = false);
	bool InputTextLocalIPOverride(const std::string_view& label_id, std::string& ip, bool requireValid = false);
	bool Combo(const char* label_id, ProgramUpdateCheckMode& mode);
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
