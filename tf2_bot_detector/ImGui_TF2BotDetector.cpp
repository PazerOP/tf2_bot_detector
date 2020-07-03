#include "ImGui_TF2BotDetector.h"
#include "CachedVariable.h"
#include "Clock.h"
#include "Networking/NetworkHelpers.h"
#include "PathUtils.h"
#include "Config/Settings.h"
#include "RegexHelpers.h"
#include "SteamID.h"
#include "Platform/Platform.h"
#include "Version.h"

#include <imgui_desktop/ScopeGuards.h>
#include <mh/text/string_insertion.hpp>

#include <cstdarg>
#include <memory>
#include <regex>
#include <stdexcept>

namespace ScopeGuards = ImGuiDesktop::ScopeGuards;

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace
{
	enum class OverrideEnabled : int
	{
		Unset,
		Disabled,
		Enabled,
	};
}

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

static std::filesystem::path GetLastPathElement(const std::filesystem::path& path)
{
	auto begin = path.begin();
	auto end = path.end();

	for (auto it = end; (it--) != begin; )
	{
		if (!it->empty())
			return *it;
	}

	return {};
}

using ValidatorFn = tf2_bot_detector::DirectoryValidatorResult(*)(std::filesystem::path path);

static std::string_view GetVisibleLabel(const std::string_view& label_id)
{
	if (!label_id.empty())
	{
		auto hashIdx = label_id.find("##"sv);
		if (hashIdx != 0)
		{
			if (hashIdx == label_id.npos)
				return label_id;
			else
				return label_id.substr(0, hashIdx);
		}
	}

	return {};
}

static bool InputPathValidated(const std::string_view& label_id, const std::filesystem::path& exampleDir,
	std::filesystem::path& outPath, bool requireValid, ValidatorFn validator)
{
	ScopeGuards::ID idScope(label_id);

	bool modified = false;
	std::string pathStr;
	if (outPath.empty())
	{
		pathStr = exampleDir.string();
		modified = true;
	}
	else
	{
		pathStr = outPath.string();
	}

	constexpr char BROWSE_BTN_LABEL[] = "Browse...";
	const auto browseBtnSize = ImGui::CalcButtonSize(BROWSE_BTN_LABEL).x;

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - browseBtnSize - 8);
	modified = ImGui::InputTextWithHint("##input", exampleDir.string().c_str(), &pathStr) || modified;
	ImGui::SameLine();
	if (ImGui::Button("Browse..."))
	{
		if (auto result = Shell::BrowseForFolderDialog(); !result.empty())
		{
			pathStr = result.string();
			modified = true;
		}
	}

	if (auto label = GetVisibleLabel(label_id); !label.empty())
	{
		ImGui::SameLine(0, 4);
		ImGui::TextUnformatted(label);
	}

	bool modifySuccess = false;
	const std::filesystem::path newPath(pathStr);
	if (newPath.empty())
	{
		ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "Cannot be empty"sv);
	}
	else if (const auto validationResult = validator(newPath); !validationResult)
	{
		ImGui::TextColoredUnformatted({ 1, 0, 0, 1 },
			"Doesn't look like a real "s << GetLastPathElement(exampleDir) << " directory: " << validationResult.m_Message);
	}
	else
	{
		ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "Looks good!"sv);

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

template<typename T, typename TIsValidFunc, typename TInputFunc>
static bool OverrideControl(const std::string_view& overrideLabel, T& overrideValue,
	const T& autodetectedValue, TIsValidFunc&& isValidFunc, TInputFunc&& inputFunc)
{
	ScopeGuards::ID id(overrideLabel);

	auto storage = ImGui::GetStateStorage();
	static struct {} s_OverrideEnabled; // Unique pointer
	const auto overrideEnabledID = ImGui::GetID(&s_OverrideEnabled);

	auto& overrideEnabled = *reinterpret_cast<OverrideEnabled*>(storage->GetIntRef(overrideEnabledID, int(OverrideEnabled::Unset)));

	if (overrideEnabled == OverrideEnabled::Unset)
		overrideEnabled = isValidFunc(overrideValue) ? OverrideEnabled::Enabled : OverrideEnabled::Disabled;

	std::string checkboxLabel = "Override "s << GetVisibleLabel(overrideLabel);

	const bool isAutodetectedValueValid = isValidFunc(autodetectedValue);
	ImGui::EnabledSwitch(isAutodetectedValueValid, [&](bool enabled)
		{
			if (bool checked = (overrideEnabled == OverrideEnabled::Enabled || !enabled);
				ImGui::Checkbox(checkboxLabel.c_str(), &checked))
			{
				overrideEnabled = checked ? OverrideEnabled::Enabled : OverrideEnabled::Disabled;
			}

		}, ("Failed to autodetect value for "s << GetVisibleLabel(overrideLabel)).c_str());

	ScopeGuards::Indent indent;
	bool retVal = false;
	if (overrideEnabled == OverrideEnabled::Enabled || !isAutodetectedValueValid)
	{
		retVal = inputFunc();// , InputPathValidated(label_id, exampleDir, outPath, requireValid, validator);
	}
	else
	{
		ImGui::TextUnformatted("Autodetected value: "s << autodetectedValue);
		ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "Looks good!");
		ImGui::NewLine();

		if (overrideValue != T{})
		{
			overrideValue = T{};
			retVal = true;
		}
	}

	return retVal;
}

static bool InputTextSteamID(const char* label, SteamID& steamID, bool requireValid)
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

bool tf2_bot_detector::InputTextSteamIDOverride(const char* label, SteamID& steamID, bool requireValid)
{
	return OverrideControl("SteamID"sv, steamID, GetCurrentActiveSteamID(),
		[&](const SteamID& id) { return !requireValid || id.IsValid(); },
		[&] { return InputTextSteamID(label, steamID, requireValid); });

#if 0
	if (overrideEnabled)
	{
		if (!overrideEnabled->has_value())
			overrideEnabled->emplace(steamID.IsValid());

		ImGui::Checkbox(("Override "s << label).c_str(), &overrideEnabled->value());
	}

	if (!overrideEnabled || overrideEnabled->value())
	{

	}
	else if (steamID.IsValid())
	{
		steamID = {};
		return true;
	}

	return false;
#endif
}

static bool InputPathValidatedOverride(const std::string_view& label_id, const std::string_view& overrideLabel,
	const std::filesystem::path& exampleDir, std::filesystem::path& outPath,
	const std::filesystem::path& autodetectedPath, bool requireValid, ValidatorFn validator)
{
	return OverrideControl(overrideLabel, outPath, autodetectedPath,
		[](const std::filesystem::path& path) { return !path.empty(); },
		[&] { return InputPathValidated(label_id, exampleDir, outPath, requireValid, validator); }
	);
}

bool tf2_bot_detector::InputTextTFDirOverride(const std::string_view& label_id, std::filesystem::path& outPath,
	const std::filesystem::path& autodetectedPath, bool requireValid)
{
	static const std::filesystem::path s_ExamplePath("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf");
	return InputPathValidatedOverride(label_id, "tf directory"sv, s_ExamplePath,
		outPath, autodetectedPath, requireValid, &ValidateTFDir);
}

bool tf2_bot_detector::InputTextSteamDirOverride(const std::string_view& label_id,
	std::filesystem::path& outPath, bool requireValid)
{
	static const std::filesystem::path s_ExamplePath("C:\\Program Files (x86)\\Steam");
	return InputPathValidatedOverride(label_id, "Steam directory"sv, s_ExamplePath, outPath,
		GetCurrentSteamDir(), requireValid, &ValidateSteamDir);
}

namespace
{
	struct [[nodiscard]] IPValidatorResult
	{
		IPValidatorResult(std::string addr) : m_Address(std::move(addr)) {}

		enum class Result
		{
			Valid,

			InvalidFormat, // Doesn't match XXX.XXX.XXX.XXX
			InvalidOctetValue,  // One of the octets is < 0 or > 255
		} m_Result = Result::Valid;

		operator bool() const { return m_Result == Result::Valid; }

		std::string m_Address;
		std::string m_Message;
	};

	//static IPValidatorResult ValidateIP(std::string addr, )
}

static IPValidatorResult ValidateAndParseIPv4(std::string addr, uint8_t* values)
{
	IPValidatorResult retVal(std::move(addr));
	using Result = IPValidatorResult::Result;

	static const std::regex s_IPRegex(R"regex((\d+)\.(\d+)\.(\d+)\.(\d+))regex", std::regex::optimize);
	std::smatch match;
	if (!std::regex_match(retVal.m_Address, match, s_IPRegex))
	{
		retVal.m_Result = Result::InvalidFormat;
		retVal.m_Message = "Unknown IPv4 format, it should look something like \"192.168.1.10\"";
		return retVal;
	}

	for (size_t i = 0; i < 4; i++)
	{
		auto& octetStr = match[i + 1];
		if (auto parseResult = from_chars(octetStr, values[i]); !parseResult)
		{
			retVal.m_Result = Result::InvalidOctetValue;
			retVal.m_Message = "Octet #"s << (i + 1) << " should be between 0 and 255 inclusive";
			return retVal;
		}
	}

	return retVal;
}

static bool InputTextIPv4(std::string& addr, bool requireValid)
{
	if (ImGui::InputTextWithHint("", "XXX.XXX.XXX.XXX", &addr))
	{
		uint8_t octets[4];
		auto validation = ValidateAndParseIPv4(addr, octets);
		if (auto validation = ValidateAndParseIPv4(addr, octets); !validation)
		{
			ImGui::TextColored({ 1, 0, 0, 1 }, "Invalid IPv4 address: %s", validation.m_Message);
			return !requireValid;
		}
		else
		{
			ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "Looks good!");
			return true;
		}
	}

	return false;
}

bool tf2_bot_detector::InputTextLocalIPOverride(const std::string_view& label_id, std::string& ip, bool requireValid)
{
	static CachedVariable s_AutodetectedValue(1s, &Networking::GetLocalIP);

	static const auto IsValid = [](std::string value) -> bool
	{
		uint8_t dummy[4];
		return !!ValidateAndParseIPv4(std::move(value), dummy);
	};

	const auto InputFunc = [&]() -> bool
	{
		return InputTextIPv4(ip, true);
	};

	return OverrideControl(label_id, ip, s_AutodetectedValue.GetAndUpdate(), IsValid, InputFunc);
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

bool tf2_bot_detector::AutoLaunchTF2Checkbox(bool& value)
{
	return ImGui::Checkbox("Automatically launch TF2 when TF2 Bot Detector is opened", &value);
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
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(500);

		ImGui::TextV(tooltipFmt, args);

		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	va_end(args);
}
