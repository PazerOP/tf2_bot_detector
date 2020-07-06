#include "ChatWrappersGeneratorPage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "Log.h"
#include "Platform/Platform.h"

#include <mh/future.hpp>
#include <mh/text/string_insertion.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <random>

using namespace std::string_literals;
using namespace tf2_bot_detector;

bool ChatWrappersGeneratorPage::ValidateSettings(const Settings& settings) const
{
	return settings.m_Unsaved.m_ChatMsgWrappers.has_value();
}

auto ChatWrappersGeneratorPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	if (!m_ChatWrappersGenerated.valid())
		return OnDrawResult::EndDrawing; // Nothing to wait for

	if (Processes::IsTF2Running())
	{
		if (m_WasInitiallyClosed)
		{
			ImGui::TextColoredUnformatted({ 1, 1, 0, 1 },
				"TF2 was opened during chat wrapper generation. You must close TF2 before continuing.\n"
				"\n"
				"tl;dr: close tf2");
		}
		else
		{
			ImGui::TextColoredUnformatted({ 1, 1, 0, 1 },
				"Existing chat wrappers were not found, so TF2 must be closed during chat wrapper generation.\n"
				"\n"
				"tl;dr: close tf2");
		}
	}
	else
	{
		ImGui::TextUnformatted("Generating chat message wrappers...");

		if (mh::is_future_ready(m_ChatWrappersGenerated))
			return OnDrawResult::EndDrawing;
	}

	ImGui::NewLine();
	if (AutoLaunchTF2Checkbox(ds.m_Settings->m_AutoLaunchTF2))
		ds.m_Settings->SaveFile();

	return OnDrawResult::ContinueDrawing;
}

static std::filesystem::path GetSavedChatMsgWrappersFilename(const std::filesystem::path& tfDir)
{
	return tfDir / "custom" / TF2BD_CHAT_WRAPPERS_DIR / "__tf2bd_chat_msg_wrappers.json";
}

static std::optional<ChatWrappers> TryLoadChatWrappers(const std::filesystem::path& tfDir)
{
	const auto path = GetSavedChatMsgWrappersFilename(tfDir);

	nlohmann::json json;
	{
		std::ifstream file(path);
		if (!file.good())
			return std::nullopt;

		file >> json;
	}

	return json.at("wrappers").get<ChatWrappers>();
}

void ChatWrappersGeneratorPage::Init(const Settings& settings)
{
	const auto tfDir = settings.GetTFDir();

	m_WasInitiallyClosed = !Processes::IsTF2Running();
	if (!m_WasInitiallyClosed)
	{
		DebugLog("TF2 was found running, trying to load saved chat wrappers...");
		try
		{
			m_ChatWrappersLoaded = TryLoadChatWrappers(tfDir);
		}
		catch (const std::exception& e)
		{
			DebugLogWarning("Failed to load existing chat message wrappers: "s << typeid(e).name() << ": " << e.what());
		}
	}

	if (!m_ChatWrappersLoaded)
	{
		if (auto existingWrappers = GetSavedChatMsgWrappersFilename(tfDir);
			std::filesystem::remove(existingWrappers))
		{
			DebugLog("Removed existing chat wrappers from "s << existingWrappers);
		}

		DebugLog("Regenerating chat wrappers...");
		m_ChatWrappersGenerated = std::async([tfDir] { return RandomizeChatWrappers(tfDir); });
	}
}

bool ChatWrappersGeneratorPage::CanCommit() const
{
	return true;
}

void ChatWrappersGeneratorPage::Commit(Settings& settings)
{
	const auto wrappersFolder = settings.GetTFDir() / "custom" / TF2BD_CHAT_WRAPPERS_DIR;

	if (m_ChatWrappersGenerated.valid())
	{
		assert(mh::is_future_ready(m_ChatWrappersGenerated));
		settings.m_Unsaved.m_ChatMsgWrappers = m_ChatWrappersGenerated.get();

		// Write chat wrappers
		{
			nlohmann::json json;
			json =
			{
				{ "wrappers", settings.m_Unsaved.m_ChatMsgWrappers.value() }
			};

			auto jsonFileName = GetSavedChatMsgWrappersFilename(settings.GetTFDir());
			std::ofstream jsonFile(jsonFileName, std::ios::trunc | std::ios::binary);

			auto jsonStr = json.dump(1, '\t', true);
			jsonFile << jsonStr;
		}
	}
	else
	{
		settings.m_Unsaved.m_ChatMsgWrappers = m_ChatWrappersLoaded.value();
	}

	// Generate a random token that will be used to verify that our
	// custom chat wrappers are in the search path
	{
		std::random_device generator;
		settings.m_Unsaved.m_ChatMsgWrappersToken = generator();
	}

	// Write a cfg file
	{
		auto folder = wrappersFolder / "cfg/";
		std::filesystem::create_directories(folder);

		auto fileName = folder / VERIFY_CFG_FILE_NAME;
		std::ofstream file(fileName, std::ios::trunc);
		file << "echo " << GetChatWrapperStringToken(settings.m_Unsaved.m_ChatMsgWrappersToken);
	}
}

std::string ChatWrappersGeneratorPage::GetChatWrapperStringToken(uint32_t token)
{
	return "TF2BD_CHAT_WRAPPER_TOKEN_VERIFY_"s << token;
}
