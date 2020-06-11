#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1

#include "FileMods.h"
#include "Log.h"
#include "TextUtils.h"

#include <vdf_parser.hpp>
#include <mh/coroutine/generator.hpp>
#include <mh/text/string_insertion.hpp>

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
	extern bool g_StaticRandomSeed;
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

static constexpr char TF2BD_LOCALIZATION_DIR[] = "aaaaaaaaaa_loadfirst_tf2_bot_detector";

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
		random.seed(unsigned(1011 + offset));
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
		ProcessWrapper(type.m_Full.first);
		ProcessWrapper(type.m_Full.second);
		ProcessWrapper(type.m_Name.first);
		ProcessWrapper(type.m_Name.second);
		ProcessWrapper(type.m_Message.first);
		ProcessWrapper(type.m_Message.second);
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
		*category = ChatCategory::TeamSpec;
	else if (*name == "AllSpec")
		*category = ChatCategory::AllSpec;
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
	const auto replaceStr = "$1"s << wrapper.m_Full.first << "$2"
		<< wrapper.m_Name.first << "%s1" << wrapper.m_Name.second
		<< "$3"
		<< wrapper.m_Message.first << "%s2" << wrapper.m_Message.second
		<< "$4"
		<< wrapper.m_Full.second;

	auto replaced = std::regex_replace(translation, s_Regex, replaceStr);
	if (replaced == translation)
	{
		LogError("Failed to apply chat message regex to "s << std::quoted(translation));
		return;
	}

	translation = std::move(replaced);
}

[[nodiscard]] static bool ApplyChatWrappersToFiles(const std::filesystem::path& src,
	const std::filesystem::path& dest, const ChatWrappers& wrappers)
{
	DebugLog("Writing modified "s << src << " to " << dest);

	auto translations = ToMB(ReadWideFile(src));

	auto begin = translations.data();
	auto end = begin + translations.size();
	auto values = tyti::vdf::read(begin, end);

	if (auto tokens = values.childs["Tokens"])
	{
		for (auto& token : tokens->attribs)
		{
			ChatCategory cat;
			if (!GetChatCategory(&token.first, nullptr, &cat, nullptr))
				continue;

			ApplyChatWrappers(cat, token.second, wrappers);
		}
	}
	else
	{
		LogError(std::string(__FUNCTION__ ": No tokens found in ") << src);
	}

	translations.clear();
	mh::strwrapperstream stream(translations);
	tyti::vdf::write(stream, values);
}

static std::string_view IsLocalizationFile(const std::filesystem::path& file)
{
	auto filename = file.filename();

	const auto ext = filename.extension();
	if (ext != ".txt")
		return {};

	auto filenameStr = std::filesystem::path(filename).replace_extension().string();
	if (filenameStr.starts_with("chat_"))
		filenameStr.erase(0, 5);
	else
		return {};

	for (const auto& lang : LANGUAGES)
	{
		if (filenameStr == lang)
			return lang;
	}

	return {};
}

namespace
{
	struct ResourceFilePath
	{
		std::string_view m_Language;
		std::filesystem::path m_RelativePath;
		std::filesystem::path m_FullPath;

		bool operator<(const ResourceFilePath& other) const { return m_Language < other.m_Language; }
		bool operator==(const ResourceFilePath& other) const { return m_Language == other.m_Language; }
	};
}

static mh::generator<std::filesystem::path> GetLocalizationFiles(
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

		if (customDir.filename() == TF2BD_LOCALIZATION_DIR)
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
		case ChatCategory::AllSpec:   return "[english]TF_Chat_AllSpec"sv;
		case ChatCategory::TeamSpec:  return "[english]TF_Chat_Spec"sv;
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
		case ChatCategory::AllSpec:   return "TF_Chat_AllSpec"sv;
		case ChatCategory::TeamSpec:  return "TF_Chat_Spec"sv;
		case ChatCategory::Coach:     return "TF_Chat_Coach"sv;
		}
	}

	LogError(std::string(__FUNCTION__ ": Unknown key for ") << cat << " with isEnglish = " << isEnglish);
	return "TF_Chat_UNKNOWN"sv;
}

ChatWrappers tf2_bot_detector::RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars)
{
	if (auto path = tfdir / "custom" / "tf2_bot_detector"; std::filesystem::exists(path))
	{
		Log("Deleting "s << path);
		std::filesystem::remove(path);
	}

	ChatWrappers wrappers(wrapChars);

	const auto outputDir = tfdir / "custom" / TF2BD_LOCALIZATION_DIR / "resource";
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
