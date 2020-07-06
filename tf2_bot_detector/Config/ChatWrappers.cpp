#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1

#include "ChatWrappers.h"
#include "Log.h"
#include "TextUtils.h"
#include "Config/JSONHelpers.h"

#include <vdf_parser.hpp>
#include <cppcoro/generator.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <execution>
#include <random>
#include <regex>
#include <set>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern uint32_t g_StaticRandomSeed;
}
#endif

static constexpr std::string_view LANGUAGES[] =
{
	"brazilian",
	"bulgarian",
	"czech",
	"danish",
	"dutch",
	"english",
	"finnish",
	"french",
	"german",
	"greek",
	"hungarian",
	"italian",
	"japanese",
	"korean",
	"koreana",
	"norwegian",
	"pirate",
	"polish",
	"portuguese",
	"romanian",
	"russian",
	"schinese",
	"spanish",
	"swedish",
	"tchinese",
	"thai",
	"turkish",
	"ukrainian",
};

static constexpr const char8_t* INVISIBLE_CHARS[] =
{
	//u8"\u180E",
	u8"\u200B",
	u8"\u200C",
	u8"\u200D",
	u8"\u2060",
};

namespace
{
	struct ChatFormatStrings
	{
		using array_t = std::array<std::string, (size_t)ChatCategory::COUNT>;
		array_t m_English;
		array_t m_Localized;
	};
}

static std::string GenerateInvisibleCharSequence(int offset, size_t wrapChars)
{
	std::mt19937 random;
#ifdef _DEBUG
	if (g_StaticRandomSeed)
	{
		random.seed(unsigned(g_StaticRandomSeed + offset));
	}
	else
#endif
	{
		random.seed(unsigned(std::chrono::high_resolution_clock::now().time_since_epoch().count() + offset));
	}

	std::uniform_int_distribution<size_t> dist(0, std::size(INVISIBLE_CHARS) - 1);

	std::string narrow;
	for (size_t i = 0; i < wrapChars; i++)
	{
		const auto randomIndex = dist(random);
		const char* bytes = reinterpret_cast<const char*>(INVISIBLE_CHARS[randomIndex]);
		narrow += bytes;
		//wide += ToU16(bytes);
	}

	return narrow;
}

ChatWrappers::ChatWrappers(size_t wrapChars)
{
	std::set<wrapper_t> generated;

	int i = 0;
	const auto ProcessWrapper = [&](wrapper_t& wrapper)
	{
		std::string seq;
		do
		{
			seq = GenerateInvisibleCharSequence(i++, wrapChars);

		} while (generated.contains(seq));

		generated.insert(seq);
		wrapper = std::move(seq);
	};

	for (Type& type : m_Types)
	{
		ProcessWrapper(type.m_Full.m_Start);
		ProcessWrapper(type.m_Full.m_End);
		ProcessWrapper(type.m_Name.m_Start);
		ProcessWrapper(type.m_Name.m_End);
		ProcessWrapper(type.m_Message.m_Start);
		ProcessWrapper(type.m_Message.m_End);
	}
}

template<typename TFunc>
static void RegexSmartReplace(std::string& str, const std::regex& regex, TFunc&& func)
{
	auto it = str.begin();
	std::match_results<std::string::iterator> match;
	while (std::regex_search(it, str.end(), match, regex))
	{
		const auto& cmatch = match;
		std::string newStr = func(cmatch);

		size_t offs = match[0].first - str.begin();
		str.replace(match[0].first, match[0].second, newStr);
		it = str.begin() + offs + newStr.size();
	}
}

static bool GetChatCategory(const std::string* src, std::string_view* name, ChatCategory* category, bool* isEnglish)
{
	if (!src)
	{
		LogError("nullptr passed to "s << __FUNCTION__);
		return false;
	}

	static constexpr auto TF_CHAT_ROOT = "TF_Chat_"sv;
	static constexpr auto ENGLISH_TF_CHAT_ROOT = "[english]TF_Chat_"sv;

	std::string_view localName;
	if (!name)
		name = &localName;

	if (src->starts_with(TF_CHAT_ROOT))
	{
		*name = std::string_view(*src).substr(TF_CHAT_ROOT.size());
		if (isEnglish)
			*isEnglish = false;
	}
	else if (src->starts_with(ENGLISH_TF_CHAT_ROOT))
	{
		*name = std::string_view(*src).substr(ENGLISH_TF_CHAT_ROOT.size());
		if (isEnglish)
			*isEnglish = true;
	}
	else
	{
		return false;
	}

	ChatCategory localCategory;
	if (!category)
		category = &localCategory;

	if (*name == "Team")
		*category = ChatCategory::Team;
	else if (*name == "Team_Dead")
		*category = ChatCategory::TeamDead;
	else if (*name == "Spec")
		*category = ChatCategory::SpecTeam;
	else if (*name == "AllSpec")
		*category = ChatCategory::Spec;
	else if (*name == "All")
		*category = ChatCategory::All;
	else if (*name == "AllDead")
		*category = ChatCategory::AllDead;
	else if (*name == "Coach")
		*category = ChatCategory::Coach;
	else if (*name == "Team_Loc" || *name == "Party")
		return false;
	else
	{
		LogError("Unknown chat type localization string "s << std::quoted(*name));
		return false;
	}

	return true;
}

static void GetChatMsgFormats(const std::string_view& debugInfo, const std::string_view& translations, ChatFormatStrings& strings)
{
	const char* begin = translations.data();
	const char* end = begin + translations.size();
	std::error_code ec;
	auto parsed = tyti::vdf::read(begin, end, ec);
	if (ec)
	{
		LogError("Failed to parse translations from "s << std::quoted(debugInfo) << ": " << ec);
		return;
	}

	if (auto tokens = parsed.childs["Tokens"])
	{
		std::string_view chatType;
		bool isEnglish;

		for (const auto& attrib : tokens->attribs)
		{
			ChatCategory cat;
			if (!GetChatCategory(&attrib.first, &chatType, &cat, &isEnglish))
				continue;

			(isEnglish ? strings.m_English : strings.m_Localized)[(int)cat] = attrib.second;
		}
	}
}

static void ApplyChatWrappers(ChatCategory cat, std::string& translation, const ChatWrappers& wrappers)
{
	static const std::basic_regex s_Regex(R"regex(([\x01-\x05]?)(.*)%s1(.*)%s2(.*))regex",
		std::regex::optimize | std::regex::icase);

	const auto& wrapper = wrappers.m_Types[(int)cat];
	const auto replaceStr = "$1"s << wrapper.m_Full.m_Start << "$2"
		<< wrapper.m_Name.m_Start << "%s1" << wrapper.m_Name.m_End
		<< "$3"
		<< wrapper.m_Message.m_Start << "%s2" << wrapper.m_Message.m_End
		<< "$4"
		<< wrapper.m_Full.m_End;

	auto replaced = std::regex_replace(translation, s_Regex, replaceStr);
	if (replaced == translation)
	{
		LogError("Failed to apply chat message regex to "s << std::quoted(translation));
		return;
	}

	translation = std::move(replaced);
}

static cppcoro::generator<std::filesystem::path> GetLocalizationFiles(
	const std::filesystem::path& tfDir, const std::string_view& language, bool baseFirst = true)
{
	if (baseFirst)
	{
		if (auto path = tfDir / "resource" / ("tf_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
		if (auto path = tfDir / "resource" / ("chat_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
	}

	for (const auto& customDirEntry : std::filesystem::directory_iterator(tfDir / "custom"))
	{
		if (!customDirEntry.is_directory())
			continue;

		const auto customDir = customDirEntry.path();
		if (!std::filesystem::exists(customDir / "resource"))
			continue;

		if (customDir.filename() == TF2BD_CHAT_WRAPPERS_DIR)
			continue;

		if (auto path = customDir / "resource" / ("tf_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
		if (auto path = customDir / "resource" / ("chat_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
	}

	if (!baseFirst)
	{
		if (auto path = tfDir / "resource" / ("tf_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
		if (auto path = tfDir / "resource" / ("chat_"s << language << ".txt"); std::filesystem::exists(path))
			co_yield path;
	}
}

static std::filesystem::path FindExistingChatTranslationFile(
	const std::filesystem::path& tfDir, const std::string_view& language)
{
	const auto desiredFilename = "chat_"s << language << ".txt";
	for (const auto& path : GetLocalizationFiles(tfDir, language, false))
	{
		//Log("Filename: "s << path.filename());
		if (path.filename() == desiredFilename)
			return path;
	}

	return {};
}

static ChatFormatStrings FindExistingTranslations(const std::filesystem::path& tfdir, const std::string_view& language)
{
	ChatFormatStrings retVal;

	for (const auto& filename : GetLocalizationFiles(tfdir, language))
		GetChatMsgFormats(filename.string(), ToMB(ReadWideFile(filename)), retVal);

	return retVal;
}

static constexpr std::string_view GetChatCategoryKey(ChatCategory cat, bool isEnglish)
{
	if (isEnglish)
	{
		switch (cat)
		{
		case ChatCategory::All:       return "[english]TF_Chat_All"sv;
		case ChatCategory::AllDead:   return "[english]TF_Chat_AllDead"sv;
		case ChatCategory::Team:      return "[english]TF_Chat_Team"sv;
		case ChatCategory::TeamDead:  return "[english]TF_Chat_Team_Dead"sv;
		case ChatCategory::Spec:   return "[english]TF_Chat_AllSpec"sv;
		case ChatCategory::SpecTeam:  return "[english]TF_Chat_Spec"sv;
		case ChatCategory::Coach:     return "[english]TF_Chat_Coach"sv;
		}
	}
	else
	{
		switch (cat)
		{
		case ChatCategory::All:       return "TF_Chat_All"sv;
		case ChatCategory::AllDead:   return "TF_Chat_AllDead"sv;
		case ChatCategory::Team:      return "TF_Chat_Team"sv;
		case ChatCategory::TeamDead:  return "TF_Chat_Team_Dead"sv;
		case ChatCategory::Spec:   return "TF_Chat_AllSpec"sv;
		case ChatCategory::SpecTeam:  return "TF_Chat_Spec"sv;
		case ChatCategory::Coach:     return "TF_Chat_Coach"sv;
		}
	}

	LogError(std::string(__FUNCTION__ ": Unknown key for ") << cat << " with isEnglish = " << isEnglish);
	return "TF_Chat_UNKNOWN"sv;
}

static void PrintChatWrappers(const ChatWrappers& wrappers)
{
	static const auto Indent = [](std::string& str, int levels)
	{
		while (levels--)
			str.push_back('\t');
	};

	using wrapper_pair_t = ChatWrappers::wrapper_pair_t;
	static const auto PrintWrapper = [](std::string& str, const std::string_view& wrapper)
	{
		mh::strwrapperstream os(str);
		os << std::quoted(wrapper) << " (";
		os << std::hex << std::uppercase;
		for (auto c : wrapper)
			os << "\\x" << (+c & 0xFF);

		os << ')';
	};
	const auto PrintWrapperPair = [](std::string& str, int indentLevels, const wrapper_pair_t& wrapper)
	{
		str << '\n';

		Indent(str, indentLevels + 1);
		str << "begin: ";
		PrintWrapper(str, wrapper.m_Start);

		str << '\n';
		Indent(str, indentLevels + 1);
		str << "end:   ";
		PrintWrapper(str, wrapper.m_End);
	};

	std::string logMsg = "Generated chat message wrappers:";// for "s << cat << '\n';
	for (int i = 0; i < (int)ChatCategory::COUNT; i++)
	{
		auto cat = ChatCategory(i);

		logMsg << '\n';
		Indent(logMsg, 1);
		logMsg << cat << ':';

		logMsg << "\n\t\tFull: ";
		PrintWrapperPair(logMsg, 2, wrappers.m_Types[i].m_Full);
		logMsg << "\n\t\tName: ";
		PrintWrapperPair(logMsg, 2, wrappers.m_Types[i].m_Name);
		logMsg << "\n\t\tMessage: ";
		PrintWrapperPair(logMsg, 2, wrappers.m_Types[i].m_Message);
	}
	DebugLog(std::move(logMsg));
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ChatCategory& d)
{
	switch (d)
	{
	case ChatCategory::All:       j = "all"; return;
	case ChatCategory::AllDead:   j = "all_dead"; return;
	case ChatCategory::Team:      j = "team"; return;
	case ChatCategory::TeamDead:  j = "team_dead"; return;
	case ChatCategory::Spec:      j = "spec"; return;
	case ChatCategory::SpecTeam:  j = "spec_team"; return;
	case ChatCategory::Coach:     j = "coach"; return;

	default:
		throw std::invalid_argument("Invalid ChatCategory("s << std::underlying_type_t<ChatCategory>(d) << ')');
	}
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ChatCategory& d)
{
	if (j == "all")
		d = ChatCategory::All;
	else if (j == "all_dead")
		d = ChatCategory::AllDead;
	else if (j == "team")
		d = ChatCategory::Team;
	else if (j == "team_dead")
		d = ChatCategory::TeamDead;
	else if (j == "spec")
		d = ChatCategory::Spec;
	else if (j == "spec_team")
		d = ChatCategory::SpecTeam;
	else if (j == "coach")
		d = ChatCategory::Coach;
	else
		throw std::invalid_argument("Unknown ChatCategory "s << std::quoted(j.get<std::string_view>()));
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ChatWrappers::WrapperPair& d)
{
	j =
	{
		{ "start", d.m_Start },
		{ "end", d.m_End },
	};
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ChatWrappers::WrapperPair& d)
{
	j.at("start").get_to(d.m_Start);
	j.at("end").get_to(d.m_End);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ChatWrappers::Type& d)
{
	j =
	{
		{ "full", d.m_Full },
		{ "name", d.m_Name },
		{ "message", d.m_Message },
	};
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ChatWrappers::Type& d)
{
	j.at("full").get_to(d.m_Full);
	j.at("name").get_to(d.m_Name);
	j.at("message").get_to(d.m_Message);
}

void tf2_bot_detector::to_json(nlohmann::json& j, const ChatWrappers& d)
{
	j = nlohmann::json::array();

	for (size_t i = 0; i < std::size(d.m_Types); i++)
	{
		auto& elem = j.emplace_back(d.m_Types[i]);
		elem["type"] = ChatCategory(i);
	}
}

void tf2_bot_detector::from_json(const nlohmann::json& j, ChatWrappers& d)
{
	for (const auto& obj : j)
	{
		auto type = (ChatCategory)obj.at("type");
		d.m_Types.at(size_t(type)) = obj;
	}
}

ChatWrappers tf2_bot_detector::RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars)
{
	assert(!tfdir.empty());

	if (auto path = tfdir / "custom" / "tf2_bot_detector"; std::filesystem::exists(path))
	{
		Log("Deleting "s << path);
		std::filesystem::remove_all(path);
	}

	ChatWrappers wrappers(wrapChars);
	PrintChatWrappers(wrappers);

	const auto outputDir = tfdir / "custom" / TF2BD_CHAT_WRAPPERS_DIR / "resource";
	std::filesystem::create_directories(outputDir);

	std::for_each(std::execution::par_unseq, std::begin(LANGUAGES), std::end(LANGUAGES),
		[&](const std::string_view& lang)
		{
			auto translationsSet = FindExistingTranslations(tfdir, lang);

			// Create our own chat localization file
			using obj = tyti::vdf::object;
			obj file;
			file.name = "lang";
			file.add_attribute("Language", std::string(lang));
			auto& tokens = file.childs["Tokens"] = std::make_shared<obj>();
			tokens->name = "Tokens";

			// Copy all from existing
			if (auto existing = FindExistingChatTranslationFile(tfdir, lang); !existing.empty())
			{
				auto baseFile = ToMB(ReadWideFile(existing));

				std::error_code ec;
				auto values = tyti::vdf::read(baseFile.begin(), baseFile.end(), ec);
				if (ec)
				{
					LogError("Failed to parse keyvalues from "s << existing);
				}
				else if (auto existingTokens = values.childs["Tokens"])
				{
					for (auto& attrib : existingTokens->attribs)
						tokens->attribs[attrib.first] = attrib.second;
				}
				else
				{
					LogError("Missing \"Tokens\" key in "s << existing);
				}
			}

			for (size_t i = 0; i < translationsSet.m_Localized.size(); i++)
			{
				ApplyChatWrappers(ChatCategory(i), translationsSet.m_Localized[i], wrappers);
				const auto key = GetChatCategoryKey(ChatCategory(i), false);
				tokens->attribs[std::string(key)] = translationsSet.m_Localized[i];
			}
			for (size_t i = 0; i < translationsSet.m_English.size(); i++)
			{
				if (translationsSet.m_English[i].empty())
					continue;

				ApplyChatWrappers(ChatCategory(i), translationsSet.m_English[i], wrappers);
				const auto key = GetChatCategoryKey(ChatCategory(i), false);
				tokens->attribs[std::string(key)] = translationsSet.m_English[i];
			}

			std::string outFile;
			mh::strwrapperstream stream(outFile);
			tyti::vdf::write(stream, file);

			WriteWideFile(outputDir / ("chat_"s << lang << ".txt"), ToU16(outFile));
		});

	Log("Wrote "s << std::size(LANGUAGES) << " modified translations to " << outputDir);

	return std::move(wrappers);
}
