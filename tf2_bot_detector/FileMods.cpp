#include "FileMods.h"
#include "Log.h"
#include "TextUtils.h"

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

static constexpr const char* LANGUAGES[] =
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

// http://emptycharacter.com/
static constexpr const char8_t* WRAP_CHARS[] =
{
	//u8"\u0020",
	//u8"\u00A0",
	//u8"\u2000",
	//u8"\u2001",
	//u8"\u2002",
	//u8"\u2003",
	//u8"\u2004",
	//u8"\u2005",
	//u8"\u2006",
	//u8"\u2007",
	//u8"\u2008",
	//u8"\u2009",
	//u8"\u200A",
	////u8"\u2028",
	//u8"\u205F",
	//u8"\u3000",
	//u8"\uFEFF",

	//u8"\u180E",
	u8"\u200B",
	u8"\u200C",
	u8"\u200D",
	u8"\u2060",
};

static constexpr char TF2BD_LOCALIZATION_DIR[] = "zzzzzzzz_loadlast_tf2_bot_detector";

namespace
{
	struct ChatWrappers final
	{
		ChatWrappers(size_t wrapChars = 16)
		{
			GenerateCharSequence(m_BeginNarrow, m_BeginWide, 0, wrapChars);
			do
			{
				GenerateCharSequence(m_EndNarrow, m_EndWide, 1, wrapChars);
			} while (m_EndWide == m_BeginWide);
		}

		std::string m_BeginNarrow, m_EndNarrow;
		std::u16string m_BeginWide, m_EndWide;

	private:
		static void GenerateCharSequence(std::string& narrow, std::u16string& wide, int offset, size_t wrapChars)
		{
			narrow.clear();
			wide.clear();

			std::string seq;

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
			std::uniform_int_distribution<size_t> dist(0, std::size(WRAP_CHARS) - 1);
			for (size_t i = 0; i < wrapChars; i++)
			{
				//std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
				const auto randomIndex = dist(random);
				const char* bytes = reinterpret_cast<const char*>(WRAP_CHARS[randomIndex]);
				narrow += bytes;
				wide += ToU16(bytes);
			}
		}
	};
}

static void ApplyChatWrappers(const std::filesystem::path& src, const std::filesystem::path& dest, const ChatWrappers& wrappers)
{
	static const std::basic_regex s_Regex(R"regex("(\[english\])?TF_Chat_(.*?)"\s+"([\x01-\x05]?)(.*)")regex",
		std::regex::optimize | std::regex::icase);

	DebugLog("Writing modified "s << src << " to " << dest);

	auto translations = ToMB(ReadWideFile(src));

	const auto replaceStr = "\"$1TF_Chat_$2\" \"$3"s << wrappers.m_BeginNarrow << "$4" << wrappers.m_EndNarrow << '"';

	translations = std::regex_replace(translations.c_str(), s_Regex, replaceStr.c_str());

	WriteWideFile(dest, ToU16(translations));
}

static bool IsLocalizationFile(const std::filesystem::path& file)
{
	auto filename = file.filename();

	const auto ext = filename.extension();
	if (ext != ".txt")
		return false;

	auto filenameStr = std::filesystem::path(filename).replace_extension().string();
	if (filenameStr.starts_with("chat_"))
		filenameStr.erase(0, 5);
	else if (filenameStr.starts_with("tf_"))
		filenameStr.erase(0, 3);
	else
		return false;

	for (const auto& lang : LANGUAGES)
	{
		if (filenameStr == lang)
			return true;
	}

	return false;
}

namespace
{
	struct ResourceFilePath
	{
		std::filesystem::path m_RelativePath;
		std::filesystem::path m_FullPath;

		bool operator<(const ResourceFilePath& other) const { return m_RelativePath < other.m_RelativePath; }
		bool operator==(const ResourceFilePath& other) const { return m_RelativePath == other.m_RelativePath; }
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

			if (IsLocalizationFile(entry))
				retVal.insert({ .m_RelativePath = std::filesystem::relative(entry, dir), .m_FullPath = entry });
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

std::pair<std::string, std::string> tf2_bot_detector::RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars)
{
	ChatWrappers wrappers;

	const auto translationsSet = FindExistingTranslations(tfdir);

	const auto inputDir = tfdir / "resource";
	const auto outputDir = tfdir / "custom/zzzzzzzz_loadlast_tf2_bot_detector/resource";
	std::filesystem::create_directories(outputDir);

	std::for_each(std::execution::par_unseq, std::begin(translationsSet), std::end(translationsSet),
		[&](const ResourceFilePath& file)
		{
			ApplyChatWrappers(file.m_FullPath, outputDir / file.m_RelativePath, wrappers);
		});

	Log("Wrote "s << std::size(translationsSet) << " modified translations to " << outputDir);

	return { std::move(wrappers.m_BeginNarrow), std::move(wrappers.m_EndNarrow) };
}
