#include "TF2CommandLinePage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "PlatformSpecific/Processes.h"
#include "PlatformSpecific/Shell.h"

#include <mh/text/string_insertion.hpp>

#include <chrono>
#include <random>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

#ifdef _DEBUG
namespace tf2_bot_detector
{
	extern uint32_t g_StaticRandomSeed;
}
#endif

static std::string GenerateRandomRCONPassword(size_t length = 16)
{
	std::mt19937 generator;
#ifdef _DEBUG
	if (g_StaticRandomSeed != 0)
	{
		generator.seed(g_StaticRandomSeed);
	}
	else
#endif
	{
		std::random_device randomSeed;
		generator.seed(randomSeed());
	}

	constexpr char PALETTE[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	std::uniform_int_distribution<size_t> dist(0, std::size(PALETTE) - 1);

	std::string retVal(length, '\0');
	for (size_t i = 0; i < length; i++)
		retVal[i] = PALETTE[dist(generator)];

	return retVal;
}

static uint16_t GenerateRandomRCONPort()
{
	std::mt19937 generator;
#ifdef _DEBUG
	if (g_StaticRandomSeed != 0)
	{
		generator.seed(g_StaticRandomSeed + 314);
	}
	else
#endif
	{
		std::random_device randomSeed;
		generator.seed(randomSeed());
	}

	// Some routers have issues handling high port numbers. By restricting
	// ourselves to these high port numbers, we add another layer of security.
	std::uniform_int_distribution<uint16_t> dist(40000, 65535);
	return dist(generator);
}

void TF2CommandLinePage::TryUpdateCmdlineArgs()
{
	if (m_CommandLineArgsFuture.valid() &&
		m_CommandLineArgsFuture.wait_for(0s) == std::future_status::ready)
	{
		m_CommandLineArgs = m_CommandLineArgsFuture.get();
		m_CommandLineArgsFuture = {};
		m_Ready = true;
	}

	if (!m_CommandLineArgsFuture.valid())
	{
		// See about starting a new update

		const auto curTime = clock_t::now();
		if (!m_Ready || (curTime >= (m_LastCLUpdate + CL_UPDATE_INTERVAL)))
		{
			m_CommandLineArgsFuture = Processes::GetTF2CommandLineArgsAsync();
			m_LastCLUpdate = curTime;
		}
	}
}

bool TF2CommandLinePage::HasUseRconCmdLineFlag() const
{
	if (m_CommandLineArgs.size() != 1)
		return false;

	return m_CommandLineArgs.at(0).find("-usercon") != std::string::npos;
}

bool TF2CommandLinePage::ValidateSettings(const Settings& settings) const
{
	if (!Processes::IsTF2Running())
		return false;
	if (!HasUseRconCmdLineFlag())
		return false;

	return true;
}

auto TF2CommandLinePage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	TryUpdateCmdlineArgs();

	const auto LaunchTF2Button = [&]
	{
		ImGui::NewLine();
		ImGui::EnabledSwitch(m_Ready, [&]
			{
				if (ImGui::Button("Launch TF2"))
				{
					std::string url;
					url << "steam://run/440//"
						" -usercon"
						" +ip 0.0.0.0 +alias ip"
						" +sv_rcon_whitelist_address 127.0.0.1 +alias sv_rcon_whitelist_address"
						" +rcon_password " << m_RCONPassword <<
						" +hostport " << m_RCONPort << " +alias hostport"
						" +con_timestamp 1 +alias con_timestamp"
						" +net_start"
						" -condebug"
						" -conclearlog";

					Shell::OpenURL(std::move(url));
				}
			}, "Finding command line arguments...");
	};

	if (m_CommandLineArgs.empty())
	{
		ImGui::TextUnformatted("Waiting for TF2 to be opened...");
		LaunchTF2Button();
	}
	else if (m_CommandLineArgs.size() > 1)
	{
		ImGui::TextUnformatted("More than one instance of hl2.exe found. Please close the other instances.");
	}
	else if (!HasUseRconCmdLineFlag())
	{
		ImGui::TextUnformatted("TF2 must be run with the -usercon command line flag. You can either add that flag under Launch Options in Steam, or close TF2 and open it with the button below.");

		ImGui::EnabledSwitch(false, LaunchTF2Button, "TF2 is currently running. Please close it first.");
	}
	else
	{
		return OnDrawResult::EndDrawing;
	}

	return OnDrawResult::ContinueDrawing;
}

void TF2CommandLinePage::Init(const Settings& settings)
{
	m_RCONPassword = GenerateRandomRCONPassword();
	m_RCONPort = GenerateRandomRCONPort();
	m_Ready = false;
}

void TF2CommandLinePage::Commit(Settings& settings)
{
	settings.m_Unsaved.m_RCONPassword = m_RCONPassword;
	settings.m_Unsaved.m_RCONPort = m_RCONPort;
}
