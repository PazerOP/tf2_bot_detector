#include "ImGui_TF2BotDetector.h"
#include "PathUtils.h"
#include "Config/Settings.h"
#include "SteamID.h"
#include "PlatformSpecific/Shell.h"
#include "Version.h"

#include <imgui_desktop/ScopeGuards.h>
#include <mh/text/string_insertion.hpp>

#include <cstdarg>
#include <memory>
#include <stdexcept>

namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

using namespace std::string_literals;
using namespace std::string_view_literals;

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

void ImGui::TextRightAligned(const std::string_view& text, float offsetX)
{
	const auto textSize = ImGui::CalcTextSize(text.data(), text.data() + text.size());

	float cursorPosX = ImGui::GetCursorPosX();
	cursorPosX += ImGui::GetColumnWidth();// ImGui::GetContentRegionAvail().x;
	cursorPosX -= textSize.x;
	cursorPosX -= 2 * ImGui::GetStyle().ItemSpacing.x;
	cursorPosX -= offsetX;

	ImGui::SetCursorPosX(cursorPosX);
	ImGui::TextUnformatted(text);
}

void ImGui::TextRightAlignedF(const char* fmt, ...)
{
	std::va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	const int count = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);

	auto buf = std::make_unique<char[]>(count + 1);
	vsnprintf(buf.get(), count + 1, fmt, ap2);
	va_end(ap2);

	ImGui::TextRightAligned(std::string_view(buf.get(), count));
}

void ImGui::AutoScrollBox(const char* ID, ImVec2 size, void(*contentsFn)(void* userData), void* userData)
{
	if (!contentsFn)
		throw std::invalid_argument("contentsFn cannot be nullptr");

	if (ImGui::BeginChild(ID, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		const auto initialScrollY = ImGui::GetScrollY();
		const auto maxScrollY = ImGui::GetScrollMaxY();
		contentsFn(userData);

#if 0
		auto storage = ImGui::GetStateStorage();
		static struct {} s_LastScrollPercentage; // Unique pointer
		const auto lastScrollPercentageID = ImGui::GetID(&s_LastScrollPercentage);

		static struct {} s_LastScrollMaxY;       // Unique pointer
		const auto lastScrollMaxYID = ImGui::GetID(&s_LastScrollMaxY);
		const auto lastScrollMaxY = storage->GetFloat(lastScrollMaxYID);

		const auto lastScrollPercentage = storage->GetFloat(lastScrollPercentageID, 1.0f);
		if (lastScrollPercentage >= 1.0f)
			ImGui::SetScrollY(ImGui::GetScrollMaxY());

		// Save the current scroll percentage
		{
			const auto cur = ImGui::GetScrollY();
			const auto max = ImGui::GetScrollMaxY();

			float curScrollPercentage = max == 0 ? 1.0f : cur / max;
			if (lastScrollPercentage >= 1.0f && max > lastScrollMaxY)
				curScrollPercentage = 1.0f;

			storage->SetFloat(lastScrollPercentageID, curScrollPercentage);
			storage->SetFloat(lastScrollMaxYID, max);
		}
#else
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1);
#endif

#if 0
		ImGui::Text("Last scroll percentage: %f", storage->GetFloat(lastScrollPercentageID, NAN));
#endif
	}

	ImGui::EndChild();
}

bool tf2_bot_detector::InputTextSteamID(const char* label, SteamID& steamID, bool requireValid)
{
	std::string steamIDStr;
	if (steamID.IsValid())
		steamIDStr = steamID.str();

	SteamID newSID;
	const bool modified = ImGui::InputTextWithHint(label, "[U:1:1234567890]", &steamIDStr);
	bool modifySuccess = false;
	{
		if (steamIDStr.empty())
		{
			ScopeGuards::TextColor textColor({ 1, 1, 0, 1 });
			ImGui::TextUnformatted("Cannot be empty"sv);
		}
		else
		{
			try
			{
				const auto CheckValid = [&](const SteamID& id) { return !requireValid || id.IsValid(); };
				bool valid = false;
				if (modified)
				{
					newSID = SteamID(steamIDStr);
					if (CheckValid(newSID))
					{
						modifySuccess = true;
						valid = true;
					}
				}
				else
				{
					valid = CheckValid(steamID);
				}

				if (valid)
				{
					ScopeGuards::TextColor textColor({ 0, 1, 0, 1 });
					ImGui::TextUnformatted("Looks good!"sv);
				}
				else
				{
					ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
					ImGui::TextUnformatted("Invalid SteamID"sv);
				}
			}
			catch (const std::invalid_argument& error)
			{
				ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
				ImGui::TextUnformatted(error.what());
			}
		}
	}

	ImGui::NewLine();

	if (modifySuccess)
		steamID = newSID;

	return modifySuccess;
}

bool tf2_bot_detector::InputTextTFDir(const std::string_view& label_id, std::filesystem::path& outPath, bool requireValid)
{
	ScopeGuards::ID idScope(label_id);

	constexpr char EXAMPLE_TF_DIR[] = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf";

	bool modified = false;
	std::string pathStr;
	if (outPath.empty())
	{
		pathStr = EXAMPLE_TF_DIR;
		modified = true;
	}
	else
	{
		pathStr = outPath.string();
	}

	constexpr char BROWSE_BTN_LABEL[] = "Browse...";
	const auto browseBtnSize = ImGui::CalcButtonSize(BROWSE_BTN_LABEL).x;

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - browseBtnSize - 8);
	modified = ImGui::InputTextWithHint("##input", EXAMPLE_TF_DIR, &pathStr) || modified;
	ImGui::SameLine();
	if (ImGui::Button("Browse..."))
	{
		if (auto result = BrowseForFolderDialog(); !result.empty())
		{
			pathStr = result.string();
			modified = true;
		}
	}

	if (!label_id.empty())
	{
		auto hashIdx = label_id.find("##"sv);
		if (hashIdx != 0)
		{
			ImGui::SameLine(0, 4);

			if (hashIdx == label_id.npos)
				ImGui::TextUnformatted(label_id);
			else
				ImGui::TextUnformatted(label_id.substr(0, hashIdx));
		}
	}

	bool modifySuccess = false;
	const std::filesystem::path newPath(pathStr);
	if (newPath.empty())
	{
		ScopeGuards::TextColor textColor({ 1, 1, 0, 1 });
		ImGui::TextUnformatted("Cannot be empty"sv);
	}
	else if (const TFDirectoryValidator validationResult(newPath); !validationResult)
	{
		ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
		ImGui::TextUnformatted("Doesn't look like a real tf directory: "s << validationResult.m_Message);
	}
	else
	{
		ScopeGuards::TextColor textColor({ 0, 1, 0, 1 });
		ImGui::TextUnformatted("Looks good!"sv);

		if (modified)
			modifySuccess = true;
	}

	ImGui::NewLine();

	if (!requireValid)
		modifySuccess = modified;

	if (modifySuccess)
		outPath = newPath;

	return modifySuccess;
}

bool tf2_bot_detector::Combo(const char* label_id, ProgramUpdateCheckMode& mode)
{
	const char* friendlyText = "<UNKNOWN>";
	static constexpr char FRIENDLY_TEXT_DISABLED[] = "Disable automatic update checks";
	static constexpr char FRIENDLY_TEXT_PREVIEW[] = "Notify about new preview releases";
	static constexpr char FRIENDLY_TEXT_STABLE[] = "Notify about new stable releases";

	const auto oldMode = mode;

	constexpr bool allowReleases = VERSION.m_Preview == 0;
	if (!allowReleases && mode == ProgramUpdateCheckMode::Releases)
		mode = ProgramUpdateCheckMode::Previews;

	switch (mode)
	{
	case ProgramUpdateCheckMode::Disabled: friendlyText = FRIENDLY_TEXT_DISABLED; break;
	case ProgramUpdateCheckMode::Previews: friendlyText = FRIENDLY_TEXT_PREVIEW; break;
	case ProgramUpdateCheckMode::Releases: friendlyText = FRIENDLY_TEXT_STABLE; break;
	case ProgramUpdateCheckMode::Unknown:  friendlyText = "Select an option"; break;
	}

	if (ImGui::BeginCombo(label_id, friendlyText))
	{
		if (ImGui::Selectable(FRIENDLY_TEXT_DISABLED))
			mode = ProgramUpdateCheckMode::Disabled;

		ImGui::EnabledSwitch(allowReleases, [&]
			{
				ImGui::BeginGroup();

				if (ImGui::Selectable(FRIENDLY_TEXT_STABLE))
					mode = ProgramUpdateCheckMode::Releases;

				ImGui::EndGroup();
			}, "Since you are using a preview build, you will always be notified of new previews.");

		if (ImGui::Selectable(FRIENDLY_TEXT_PREVIEW))
			mode = ProgramUpdateCheckMode::Previews;

		ImGui::EndCombo();
	}

	return mode != oldMode;
}

ImVec2 ImGui::CalcButtonSize(const char* label)
{
	const auto& style = ImGui::GetStyle();
	return (ImVec2(style.FramePadding) * 2) + CalcTextSize(label);
}

void ImGui::Value(const char* prefix, double v, const char* float_format)
{
	return Value(prefix, float(v), float_format);
}

void ImGui::Value(const char* prefix, const char* str)
{
	Text("%s: %s", prefix, str ? str : "<NULL>");
}

void ImGui::Value(const char* prefix, uint64_t v)
{
	Text("%s: %llu", prefix, v);
}

void ImGui::SetHoverTooltip(const char* tooltipFmt, ...)
{
	va_list args;
	va_start(args, tooltipFmt);

	if (IsItemHovered())
		SetTooltipV(tooltipFmt, args);

	va_end(args);
}
