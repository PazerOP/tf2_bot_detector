#include "FileMods.h"
#include "Log.h"
#include "TextUtils.h"

#include <mh/coroutine/generator.hpp>
#include <mh/text/string_insertion.hpp>

#include <execution>
#include <random>
#include <regex>
#include <set>

using namespace std::string_literals;
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

static constexpr char TF2BD_LOCALIZATION_DIR[] = "zzzzzzzz_loadlast_tf2_bot_detector";

#if 0
namespace
{
	struct ChatMsgFormat final
	{
		ChatCategory m_Category{};
		bool m_Location = false;

		char m_PrePrefix = 0;
		std::string m_Prefix;
		std::string m_Mid;
		std::string m_Suffix;
	};

	struct ChatMsgFormats final
	{
		ChatMsgFormats(const std::string_view& translations);

		ChatMsgFormat& GetFormat(bool localized, const std::string_view& chatType);

		struct Language
		{
			ChatMsgFormat& GetFormat(const std::string_view& chatType);

			ChatMsgFormat m_TeamLoc;
			ChatMsgFormat m_Team;
			ChatMsgFormat m_TeamDead;
			ChatMsgFormat m_Spec;
			ChatMsgFormat m_All;
			ChatMsgFormat m_AllDead;
			ChatMsgFormat m_AllSpec;
			ChatMsgFormat m_Coach;

		} m_English, m_Localized;
	};

	ChatMsgFormats::ChatMsgFormats(const std::string_view& translations)
	{
		using it_t = std::regex_iterator<std::string_view::iterator>;
		auto it = it_t(translations.begin(), translations.end(), s_ChatMsgLocalizationRegex);
		const auto endIt = it_t{};
		for (; it != endIt; ++it)
		{
			const auto& match = *it;

			auto& format = GetFormat(match[1].matched, match[2].str());
			if (match[3].matched)
				format.m_PrePrefix = *match[3].first;

			format.m_Prefix = match[4].str();
			format.m_Mid = match[5].str();
			format.m_Suffix = match[6].str();
		}
	}
}
#endif

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

static const std::basic_regex s_ChatMsgLocalizationRegex(R"regex("(\[english\])?TF_Chat_(.*?)"\s+"([\x01-\x05]?)(.*)%s1(.*)%s2(.*)")regex",
	std::regex::optimize | std::regex::icase);

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

static std::vector<std::string> GetChatMsgFormats(const std::string_view& translations)
{
	std::vector<std::string> retVal;

	using it_t = std::regex_iterator<std::string_view::iterator>;
	auto it = it_t(translations.begin(), translations.end(), s_ChatMsgLocalizationRegex);
	const auto endIt = it_t{};
	for (; it != endIt; ++it)
		retVal.push_back((*it)[0]);

	return retVal;
}

static void ApplyChatWrappers(std::string& translations, const ChatWrappers& wrappers)
{
	using Wrapper = ChatWrappers::Type;

	RegexSmartReplace(translations, s_ChatMsgLocalizationRegex,
		[&](const std::match_results<std::string::iterator>& match)
		{
			const Wrapper* wrapper = nullptr;

			auto name = std::string_view(&*match[2].first, match[2].length());
			if (name == "Team_Loc")
				return match[0].str();
			else if (name == "Team")
				wrapper = &wrappers.m_Types[(int)ChatCategory::Team];
			else if (name == "Team_Dead")
				wrapper = &wrappers.m_Types[(int)ChatCategory::TeamDead];
			else if (name == "Spec")
				wrapper = &wrappers.m_Types[(int)ChatCategory::TeamSpec];
			else if (name == "AllSpec")
				wrapper = &wrappers.m_Types[(int)ChatCategory::AllSpec];
			else if (name == "All")
				wrapper = &wrappers.m_Types[(int)ChatCategory::All];
			else if (name == "AllDead")
				wrapper = &wrappers.m_Types[(int)ChatCategory::AllDead];
			else
			{
				LogError("Unknown chat type localization string "s << std::quoted(name));
				return match[0].str();
			}

			std::string retVal = "\""s << match[1].str() << "TF_Chat_" << match[2].str() << "\" \"" << match[3].str();

			retVal << wrapper->m_Full.first << match[4].str()
				<< wrapper->m_Name.first << "%s1" << wrapper->m_Name.second
				<< match[5].str()
				<< wrapper->m_Message.first << "%s2" << wrapper->m_Message.second
				<< wrapper->m_Full.second;

			retVal << '"';

			return retVal;
		});
}

static void ApplyChatWrappersToFiles(const std::filesystem::path& src, const std::filesystem::path& dest, const ChatWrappers& wrappers)
{
	DebugLog("Writing modified "s << src << " to " << dest);

	auto translations = ToMB(ReadWideFile(src));
	ApplyChatWrappers(translations, wrappers);
	WriteWideFile(dest, ToU16(translations));
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

namespace
{
	struct ChatFormatStrings final
	{

	};
}

static std::set<ResourceFilePath> FindExistingTranslations(const std::filesystem::path& tfdir)
{
	std::set<ResourceFilePath> retVal;

	const auto CheckResourceFolder = [&retVal](const std::filesystem::path& dir)
	{
		for (const auto& entry : std::filesystem::directory_iterator(dir))
		{
			if (!entry.is_regular_file())
				continue;

			if (auto lang = IsLocalizationFile(entry); !lang.empty())
			{
				retVal.insert(
					{
						.m_Language = lang,
						.m_RelativePath = std::filesystem::relative(entry, dir),
						.m_FullPath = entry,
					});
			}
		}
	};

	CheckResourceFolder(tfdir / "resource");

	for (const auto& entry : std::filesystem::directory_iterator(tfdir / "custom"))
	{
		if (!entry.is_directory())
			continue;

		const auto path = entry.path();
		if (!std::filesystem::exists(path / "resource"))
			continue;

		if (path.filename() == TF2BD_LOCALIZATION_DIR)
			continue;

		CheckResourceFolder(path / "resource");
	}

	return retVal;
}

ChatWrappers tf2_bot_detector::RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars)
{
	ChatWrappers wrappers(wrapChars);

	const auto translationsSet = FindExistingTranslations(tfdir);

	const auto outputDir = tfdir / "custom/zzzzzzzz_loadlast_tf2_bot_detector/resource";
	std::filesystem::create_directories(outputDir);

	const auto FindLanguage = [&](const std::string_view& language)
	{
		for (auto it = translationsSet.begin(); it != translationsSet.end(); ++it)
		{
			if (it->m_Language == language)
				return it;
		}

		return translationsSet.end();
	};

	for (const auto& lang : LANGUAGES)
	{
		if (auto found = FindLanguage(lang); found != translationsSet.end())
		{
			ApplyChatWrappersToFiles(found->m_FullPath, outputDir / found->m_RelativePath, wrappers);
		}
		else
		{
			// Create our own chat localization file
			std::string file;
			file << R"(
"lang"
{
"Language" ")" << lang << R"("
"Tokens"
{
)";

			GetChatMsgFormats()


			file << "\n}\n";

			WriteWideFile(outputDir / ("chat_"s << lang << ".txt"), ToU16(file));
		}
	}

	std::for_each(std::execution::par_unseq, std::begin(translationsSet), std::end(translationsSet),
		[&](const ResourceFilePath& file)
		{
			ApplyChatWrappersToFiles(file.m_FullPath, outputDir / file.m_RelativePath, wrappers);
		});

	Log("Wrote "s << std::size(translationsSet) << " modified translations to " << outputDir);

	return std::move(wrappers);
}
