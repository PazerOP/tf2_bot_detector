#include "ImGui_TF2BotDetector.h"
#include "PathUtils.h"
#include "SteamID.h"

#include <imgui_desktop/ScopeGuards.h>

#include <cstdarg>
#include <memory>
#include <stdexcept>

namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

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

bool tf2_bot_detector::InputTextTFDir(const char* label_id, std::filesystem::path& outPath)
{
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

	modified = ImGui::InputTextWithHint(label_id, EXAMPLE_TF_DIR, &pathStr) || modified;
	bool modifySuccess = false;
	{
		const std::filesystem::path newPath(pathStr);
		if (newPath.empty())
		{
			ScopeGuards::TextColor textColor({ 1, 1, 0, 1 });
			ImGui::TextUnformatted("Cannot be empty"sv);
		}
		else if (const TFDirectoryValidator validationResult(newPath); !validationResult)
		{
			ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
			ImGui::TextUnformatted(validationResult.m_Message);
		}
		else
		{
			ScopeGuards::TextColor textColor({ 0, 1, 0, 1 });
			ImGui::TextUnformatted("Looks good!"sv);

			if (modified)
			{
				outPath = newPath;
				return true;
			}
		}
	}

	ImGui::NewLine();

	return false;
}
