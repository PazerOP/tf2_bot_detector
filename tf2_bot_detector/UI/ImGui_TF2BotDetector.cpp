#include "ImGui_TF2BotDetector.h"
#include "Util/PathUtils.h"
#include "Clock.h"
#include "Networking/NetworkHelpers.h"
#include "Config/Settings.h"
#include "Util/RegexUtils.h"
#include "SteamID.h"
#include "Platform/Platform.h"
#include "Version.h"
#include "ReleaseChannel.h"

#include <imgui_desktop/ScopeGuards.h>
#include <mh/memory/cached_variable.hpp>
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

void ImGui::TextRightAligned(const std::string_view& text, float offsetX) //This function will right-align certain text by a certain offset. 
{
	//Because everyone has a different size monitor/display,
	//this function calcualtes your text size and the column width dynamically based on your detected system
	//graphics. Once it has these it uses a 'cursor' to properly place the right aligned text. 

	const auto textSize = ImGui::CalcTextSize(text.data(), text.data() + text.size()); //calculates the text size 

	float cursorPosX = ImGui::GetCursorPosX(); //grabs your cursor position
	cursorPosX += ImGui::GetColumnWidth();// ImGui::GetContentRegionAvail().x; //
	cursorPosX -= textSize.x;
	cursorPosX -= 2 * ImGui::GetStyle().ItemSpacing.x;
	cursorPosX -= offsetX;

	ImGui::SetCursorPosX(cursorPosX);
	ImGui::TextFmt(text);
}

void ImGui::TextRightAlignedF(const char* fmt, ...) //formats right aligned text with the fmt argument
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

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1);
	}

	ImGui::EndChild();
}

void ImGui::HorizontalScrollBox(const char* ID, void(*contentsFn)(void* userData), void* userData)
{
	ScopeGuards::ID id(ID);

	auto storage = ImGui::GetStateStorage();

	static struct {} s_ScrollerHeight; // Unique pointer
	const auto scrollerHeightID = ImGui::GetID(&s_ScrollerHeight);

	auto& height = *storage->GetFloatRef(scrollerHeightID, 1);
	if (ImGui::BeginChild("HorizontalScrollerBox", { 0, height }, false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::BeginGroup();
		contentsFn(userData);
		ImGui::EndGroup();
		const auto contentRect = ImGui::GetItemRectSize();
		const auto windowSize = ImGui::GetWindowSize();

		height = contentRect.y;
		if (contentRect.x > windowSize.x)
		{
			auto& style = ImGui::GetStyle();
			height += style.FramePadding.y + style.ScrollbarSize;
		}
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
		ImGui::TextFmt(label);
	}

	bool modifySuccess = false;
	const std::filesystem::path newPath(pathStr);
	if (newPath.empty())
	{
		ImGui::TextFmt({ 1, 1, 0, 1 }, "Cannot be empty"sv);
	}
	else if (const auto validationResult = validator(newPath); !validationResult)
	{
		ImGui::TextFmt({ 1, 0, 0, 1 }, "Doesn't look like a real {} directory: {}",
			GetLastPathElement(exampleDir), validationResult.m_Message);
	}
	else
	{
		ImGui::TextFmt({ 0, 1, 0, 1 }, "Looks good!"sv);

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

	const auto visibleLabel = GetVisibleLabel(overrideLabel);
	const bool isAutodetectedValueValid = isValidFunc(autodetectedValue);
	ImGui::EnabledSwitch(isAutodetectedValueValid, [&](bool enabled)
		{
			if (bool checked = (overrideEnabled == OverrideEnabled::Enabled || !enabled);
				ImGui::Checkbox(mh::fmtstr<512>("Override {}", visibleLabel).c_str(), &checked))
			{
				overrideEnabled = checked ? OverrideEnabled::Enabled : OverrideEnabled::Disabled;
			}

		}, mh::fmtstr<512>("Failed to autodetect value for {}", visibleLabel).c_str());

	ScopeGuards::Indent indent;
	bool retVal = false;
	if (overrideEnabled == OverrideEnabled::Enabled || !isAutodetectedValueValid)
	{
		retVal = inputFunc();// , InputPathValidated(label_id, exampleDir, outPath, requireValid, validator);
	}
	else
	{
		ImGui::TextFmt("Autodetected value: {}", autodetectedValue);
		ImGui::TextFmt({ 0, 1, 0, 1 }, "Looks good!");
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
			ImGui::TextFmt("Cannot be empty");
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
					ImGui::TextFmt("Looks good!"sv);
				}
				else
				{
					ScopeGuards::TextColor textColor({ 1, 0, 0, 1 });
					ImGui::TextFmt("Invalid SteamID"sv);
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
			ImGui::TextFmt({ 0, 1, 0, 1 }, "Looks good!");
			return true;
		}
	}

	return false;
}

bool tf2_bot_detector::InputTextLocalIPOverride(const std::string_view& label_id, std::string& ip, bool requireValid)
{
	static mh::cached_variable s_AutodetectedValue(1s, &Networking::GetLocalIP);

	static const auto IsValid = [](std::string value) -> bool
	{
		uint8_t dummy[4];
		return !!ValidateAndParseIPv4(std::move(value), dummy);
	};

	const auto InputFunc = [&]() -> bool
	{
		return InputTextIPv4(ip, true);
	};

	return OverrideControl(label_id, ip, s_AutodetectedValue.get(), IsValid, InputFunc);
}

bool tf2_bot_detector::InputTextSteamAPIKey(const char* label_id, std::string& key, bool requireValid)
{
	constexpr char BASE_INVALID_KEY_MSG[] = "Your Steam API key should be a 32 character hexadecimal string (NOT your Steam account password).";

	std::string newKey = key;
	if (ImGui::InputText(label_id, &newKey))
	{
		bool isValid = true;
		if (isValid && newKey.size() > 0 && newKey.size() != 32)
		{
			isValid = false;
			ImGui::TextColored({ 1, 0, 0, 1 },
				"%s (current length: %zu)",
				BASE_INVALID_KEY_MSG, newKey.size());
			ImGui::NewLine();
		}

		if (isValid)
		{
			const auto invalidCharIndex = newKey.find_first_not_of("0123456789abcdefABCDEF");
			if (invalidCharIndex != newKey.npos)
			{
				isValid = false;
				ImGui::TextColored({ 1, 0, 0, 1 },
					"%s (invalid character '%c')",
					BASE_INVALID_KEY_MSG, newKey.at(invalidCharIndex));
				ImGui::NewLine();
			}
		}

		if (requireValid && !isValid)
			newKey = key;
	}

	if (ImGui::Button("More Info..."))
		Platform::Shell::OpenURL("https://github.com/PazerOP/tf2_bot_detector/wiki/Integrations:-Steam-API");

	if (key.empty())
	{
		ImGui::SameLine();

		if (ImGui::Button("Generate Steam API Key"))
			Platform::Shell::OpenURL("https://steamcommunity.com/dev/apikey");
	}

	if (newKey != key)
	{
		key = newKey;
		return true;
	}

	return false;
}

bool tf2_bot_detector::Combo(const char* label_id, std::optional<ReleaseChannel>& mode)
{
	const char* friendlyText = "<UNKNOWN>";
	static constexpr char FRIENDLY_TEXT_DISABLED[] = "Disable automatic update checks";
	static constexpr char FRIENDLY_TEXT_NIGHTLY[] = "Notify about every new build (unstable)";
	static constexpr char FRIENDLY_TEXT_PREVIEW[] = "Notify about new preview releases";
	static constexpr char FRIENDLY_TEXT_STABLE[] = "Notify about new stable releases";

	const auto oldMode = mode;

#if 0
	constexpr bool allowReleases = VERSION.m_Preview == 0;
	if (!allowReleases && mode == ProgramUpdateCheckMode::Releases)
		mode = ProgramUpdateCheckMode::Previews;
#endif

	if (mode.has_value())
	{
		switch (*mode)
		{
		case ReleaseChannel::None:     friendlyText = FRIENDLY_TEXT_DISABLED; break;
		case ReleaseChannel::Nightly:  friendlyText = FRIENDLY_TEXT_NIGHTLY; break;
		case ReleaseChannel::Preview:  friendlyText = FRIENDLY_TEXT_PREVIEW; break;
		case ReleaseChannel::Public:   friendlyText = FRIENDLY_TEXT_STABLE; break;
		}
	}
	else
	{
		friendlyText = "Select an option";
	}

	if (ImGui::BeginCombo(label_id, friendlyText))
	{
		if (ImGui::Selectable(FRIENDLY_TEXT_DISABLED))
			mode = ReleaseChannel::None;

#if 0
		ImGui::EnabledSwitch(allowReleases, [&]
			{
				ImGui::BeginGroup();

				if (ImGui::Selectable(FRIENDLY_TEXT_STABLE))
					mode = ProgramUpdateCheckMode::Releases;

				ImGui::EndGroup();
			}, "Since you are using a preview build, you will always be notified of new previews.");
#else
		if (ImGui::Selectable(FRIENDLY_TEXT_STABLE))
			mode = ReleaseChannel::Public;
#endif

		if (ImGui::Selectable(FRIENDLY_TEXT_PREVIEW))
			mode = ReleaseChannel::Preview;
		if (ImGui::Selectable(FRIENDLY_TEXT_NIGHTLY))
			mode = ReleaseChannel::Nightly;

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

void ImGui::PacifierText()
{
	ImGui::TextFmt("Loading..."sv);
}
