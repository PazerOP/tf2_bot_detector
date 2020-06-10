#include "FileMods.h"
#include "Log.h"
#include "TextUtils.h"

#include <mh/coroutine/generator.hpp>
#include <mh/text/string_insertion.hpp>

#include <array>
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

static bool GetChatCategory(const std::string_view& name, ChatCategory& category)
{
	if (name == "Team")
	{
		category = ChatCategory::Team;
		return true;
	}
	else if (name == "Team_Dead")
	{
		category = ChatCategory::TeamDead;
		return true;
	}
	else if (name == "Spec")
	{
		category = ChatCategory::TeamSpec;
		return true;
	}
	else if (name == "AllSpec")
	{
		category = ChatCategory::AllSpec;
		return true;
	}
	else if (name == "All")
	{
		category = ChatCategory::All;
		return true;
	}
	else if (name == "AllDead")
	{
		category = ChatCategory::AllDead;
		return true;
	}
	else if (name == "Coach")
	{
		category = ChatCategory::Coach;
		return true;
	}
	else if (name == "Team_Loc")
	{
		return false;
	}
	else
	{
		LogError("Unknown chat type localization string "s << std::quoted(name));
		return false;
	}
}

static void GetChatMsgFormats(const std::string_view& translations, ChatFormatStrings& strings)
{
	using it_t = std::regex_iterator<std::string_view::iterator>;
	auto it = it_t(translations.begin(), translations.end(), s_ChatMsgLocalizationRegex);
	const auto endIt = it_t{};
	for (; it != endIt; ++it)
	{
		const auto& match = *it;
		if (ChatCategory cat; GetChatCategory(match[2].str(), cat))
		{
			if (match[1].matched)
				strings.m_English[(size_t)cat] = match[0].str();
			else
				strings.m_Localized[(size_t)cat] = match[0].str();
		}
	}
}

static void ApplyChatWrappers(std::string& translations, const ChatWrappers& wrappers)
{
	using Wrapper = ChatWrappers::Type;

	RegexSmartReplace(translations, s_ChatMsgLocalizationRegex,
		[&](const std::match_results<std::string::iterator>& match)
		{
			const Wrapper* wrapper = nullptr;

			auto name = std::string_view(&*match[2].first, match[2].length());
			if (ChatCategory cat; GetChatCategory(std::string_view(&*match[2].first, match[2].length()), cat))
				wrapper = &wrappers.m_Types[(int)cat];
			else
				return match[0].str();

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

static mh::generator<std::filesystem::path> GetLocalizationFiles(
	const std::filesystem::path& tfDir, const std::string_view& language)
{
	if (auto path = tfDir / "resource" / ("tf_"s << language << ".txt"); std::filesystem::exists(path))
		co_yield path;
	if (auto path = tfDir / "resource" / ("chat_"s << language << ".txt"); std::filesystem::exists(path))
		co_yield path;

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
}

static ChatFormatStrings FindExistingTranslations(const std::filesystem::path& tfdir, const std::string_view& language)
{
	ChatFormatStrings retVal;

	for (const auto& filename : GetLocalizationFiles(tfdir, language))
		GetChatMsgFormats(ToMB(ReadWideFile(filename)), retVal);

	return retVal;

#if 0
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
#endif
}

ChatWrappers tf2_bot_detector::RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars)
{
	ChatWrappers wrappers(wrapChars);

	const auto outputDir = tfdir / "custom" / TF2BD_LOCALIZATION_DIR / "resource";
	std::filesystem::create_directories(outputDir);

	std::for_each(std::execution::par_unseq, std::begin(LANGUAGES), std::end(LANGUAGES),
		[&](const std::string_view& lang)
		{
			auto translationsSet = FindExistingTranslations(tfdir, lang);

			// Create our own chat localization file
			std::string file;
			file << R"(
"lang"
{
"Language" ")" << lang << R"("
"Tokens"
{
)";

			for (auto& str : translationsSet.m_English)
			{
				if (str.empty())
					continue;

				ApplyChatWrappers(str, wrappers);
				file << str << '\n';
			}
			for (auto& str : translationsSet.m_Localized)
			{
				if (str.empty())
					continue;

				ApplyChatWrappers(str, wrappers);
				file << str << '\n';
			}

			file << "\n}\n}\n";

			WriteWideFile(outputDir / ("chat_"s << lang << ".txt"), ToU16(file));
		});

	Log("Wrote "s << std::size(LANGUAGES) << " modified translations to " << outputDir);

	return std::move(wrappers);
}
