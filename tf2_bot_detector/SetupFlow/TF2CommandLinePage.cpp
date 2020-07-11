#include "TF2CommandLinePage.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"
#include "Log.h"
#include "Platform/Platform.h"

#include <mh/future.hpp>
#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>
#include <srcon/srcon.h>

#include <chrono>
#include <random>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

static std::string GenerateRandomRCONPassword(size_t length = 16)
{
	std::mt19937 generator;
	{
		std::random_device randomSeed;
		generator.seed(randomSeed());
	}

	constexpr char PALETTE[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	std::uniform_int_distribution<size_t> dist(0, std::size(PALETTE) - 2);

	std::string retVal(length, '\0');
	for (size_t i = 0; i < length; i++)
		retVal[i] = PALETTE[dist(generator)];

	return retVal;
}

static uint16_t GenerateRandomRCONPort()
{
	std::mt19937 generator;
	{
		std::random_device randomSeed;
		generator.seed(randomSeed());
	}

	// Some routers have issues handling high port numbers. By restricting
	// ourselves to these high port numbers, we add another layer of security.
	std::uniform_int_distribution<uint16_t> dist(40000, 65535);
	return dist(generator);
}

void TF2CommandLinePage::Data::TryUpdateCmdlineArgs()
{
	if (mh::is_future_ready(m_CommandLineArgsFuture))
	{
		const auto& args = m_CommandLineArgsFuture.get();
		m_MultipleInstances = args.size() > 1;
		if (!m_MultipleInstances)
		{
			if (args.empty())
				m_CommandLineArgs.reset();
			else
				m_CommandLineArgs = TF2CommandLine::Parse(args.at(0));
		}

		m_CommandLineArgsFuture = {};
		m_AtLeastOneUpdateRun = true;
	}

	if (!m_CommandLineArgsFuture.valid())
	{
		// See about starting a new update

		const auto curTime = clock_t::now();
		if (!m_AtLeastOneUpdateRun || (curTime >= (m_LastCLUpdate + CL_UPDATE_INTERVAL)))
		{
			m_CommandLineArgsFuture = Processes::GetTF2CommandLineArgsAsync();
			m_LastCLUpdate = curTime;
		}
	}
}

bool TF2CommandLinePage::ValidateSettings(const Settings& settings) const
{
	if (!Processes::IsTF2Running())
		return false;
	if (!m_Data.m_CommandLineArgs->IsPopulated())
		return false;

	return true;
}

auto TF2CommandLinePage::TF2CommandLine::Parse(const std::string_view& cmdLine) -> TF2CommandLine
{
	const auto args = Shell::SplitCommandLineArgs(cmdLine);

	TF2CommandLine cli{};
	cli.m_FullCommandLine = cmdLine;

	for (size_t i = 0; i < args.size(); i++)
	{
		if (i < (args.size() - 1))
		{
			// We have at least one more arg
			if (cli.m_IP.empty() && args[i] == "+ip")
			{
				cli.m_IP = args[++i];
				continue;
			}
			else if (cli.m_RCONPassword.empty() && args[i] == "+rcon_password")
			{
				cli.m_RCONPassword = args[++i];
				continue;
			}
			else if (!cli.m_RCONPort.has_value() && args[i] == "+hostport")
			{
				if (uint16_t parsedPort; mh::from_chars(args[i + 1], parsedPort))
				{
					cli.m_RCONPort = parsedPort;
					i++;
					continue;
				}
			}
			else if (args[i] == "-usercon")
			{
				cli.m_UseRCON = true;
				continue;
			}
		}
	}

	return cli;
}

bool TF2CommandLinePage::TF2CommandLine::IsPopulated() const
{
	if (!m_UseRCON)
		return false;
	if (m_IP.empty())
		return false;
	if (m_RCONPassword.empty())
		return false;
	if (!m_RCONPort.has_value())
		return false;

	return true;
}

static void OpenTF2(const std::string_view& rconPassword, uint16_t rconPort)
{
	std::string url;
	url << "steam://run/440//"
		" -usercon"
		" +developer 1 +alias developer"
		" +contimes 0 +alias contimes"   // the text in the top left when developer >= 1
		" +ip 0.0.0.0 +alias ip"
		" +sv_rcon_whitelist_address 127.0.0.1 +alias sv_rcon_whitelist_address"
		" +rcon_password " << rconPassword << " +alias rcon_password"
		" +hostport " << rconPort << " +alias hostport"
		" +alias cl_reload_localization_files" // This command reloads files in backwards order, so any customizations get overwritten by stuff from the base game
		" +net_start"
		" +con_timestamp 1 +alias con_timestamp"
		" -condebug"
		" -conclearlog"
		;

	Shell::OpenURL(std::move(url));
}

TF2CommandLinePage::RCONClientData::RCONClientData(std::string pwd, uint16_t port) :
	m_Client(std::make_unique<srcon::async_client>())
{
	srcon::srcon_addr addr;
	addr.addr = "127.0.0.1";
	addr.pass = std::move(pwd);
	addr.port = port;

	m_Client->set_addr(std::move(addr));

	m_Message = "Connecting...";
}

bool TF2CommandLinePage::RCONClientData::Update()
{
	if (!m_Success)
	{
		if (mh::is_future_ready(m_Future))
		{
			try
			{
				m_Message = m_Future.get();
				m_MessageColor = { 0, 1, 0, 1 };
				m_Success = true;
			}
			catch (const srcon::srcon_error& e)
			{
				DebugLog(std::string(__FUNCTION__) << "(): " << e.what());

				using srcon::srcon_errc;
				switch (e.get_errc())
				{
				case srcon_errc::bad_rcon_password:
					m_MessageColor = { 1, 0, 0, 1 };
					m_Message = "Bad rcon password, this should never happen!";
					break;
				case srcon_errc::rcon_connect_failed:
					m_MessageColor = { 1, 1, 0.5, 1 };
					m_Message = "TF2 not yet accepting RCON connections. Retrying...";
					break;
				case srcon_errc::socket_send_failed:
					m_MessageColor = { 1, 1, 0.5, 1 };
					m_Message = "TF2 not yet accepting RCON commands...";
					break;
				default:
					m_MessageColor = { 1, 1, 0, 1 };
					m_Message = "Unexpected error: "s << e.what();
					break;
				}
				m_Future = {};
			}
			catch (const std::exception& e)
			{
				DebugLogWarning(std::string(__FUNCTION__) << "(): " << e.what());
				m_MessageColor = { 1, 0, 0, 1 };
				m_Message = "RCON connection unsuccessful: "s << e.what();
				m_Future = {};
			}
		}

		if (!m_Future.valid())
			m_Future = m_Client->send_command_async("echo RCON connection successful.", false);
	}

	ImGui::TextColoredUnformatted(m_MessageColor, m_Message);

	return m_Success;
}

void TF2CommandLinePage::DrawAutoLaunchTF2Checkbox(const DrawState& ds)
{
	if (tf2_bot_detector::AutoLaunchTF2Checkbox(ds.m_Settings->m_AutoLaunchTF2))
		ds.m_Settings->SaveFile();
}

void TF2CommandLinePage::DrawLaunchTF2Button(const DrawState& ds)
{
	const auto curTime = clock_t::now();
	const bool canLaunchTF2 = (curTime - m_Data.m_LastTF2LaunchTime) >= 2500ms;

	ImGui::NewLine();
	ImGui::EnabledSwitch(m_Data.m_AtLeastOneUpdateRun && canLaunchTF2, [&]
		{
			if ((ImGui::Button("Launch TF2") || (m_IsAutoLaunchAllowed && ds.m_Settings->m_AutoLaunchTF2)) && canLaunchTF2)
			{
				OpenTF2(m_Data.m_RandomRCONPassword, m_Data.m_RandomRCONPort);
				m_Data.m_LastTF2LaunchTime = curTime;
			}

			m_IsAutoLaunchAllowed = false;

		}, "Finding command line arguments...");

	ImGui::NewLine();
	DrawAutoLaunchTF2Checkbox(ds);
}

void TF2CommandLinePage::DrawCommandLineArgsInvalid(const DrawState& ds, const TF2CommandLine& args)
{
	m_Data.m_TestRCONClient.reset();

	if (args.m_FullCommandLine.empty())
	{
		ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "Failed to get TF2 command line arguments.");
		ImGui::NewLine();
		ImGui::TextUnformatted("This might be caused by TF2 or Steam running as administrator. Remove any"
			" compatibility options that are causing TF2 or Steam to require administrator and try again.");
	}
	else
	{
		ImGui::TextColoredUnformatted({ 1, 1, 0, 1 }, "Invalid TF2 command line arguments.");
		ImGui::NewLine();
		ImGui::TextUnformatted("TF2 must be launched via TF2 Bot Detector. Please close it, then open it again with the button below.");
		ImGui::EnabledSwitch(false, [&] { DrawLaunchTF2Button(ds); }, "TF2 is currently running. Please close it first.");

		ImGui::NewLine();

		ImGuiDesktop::ScopeGuards::GlobalAlpha alpha(0.65f);
		ImGui::TextUnformatted("Details:");
		ImGui::TextUnformatted("- Instance:");
		ImGui::SameLine();
		ImGui::TextUnformatted(args.m_FullCommandLine);

		ImGuiDesktop::ScopeGuards::Indent indent;

		if (!args.m_UseRCON)
			ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "Unable to find \"-usercon\"");
		else
			ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "-usercon");

		if (args.m_IP.empty())
			ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "Unable to find \"+ip\"");
		else
			ImGui::TextColored({ 0, 1, 0, 1 }, "+ip %s", args.m_IP.c_str());

		if (args.m_RCONPassword.empty())
			ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "Unable to find \"+rcon_password\"");
		else
			ImGui::TextColored({ 0, 1, 0, 1 }, "+rcon_password %s", args.m_RCONPassword.c_str());

		if (!args.m_RCONPort.has_value())
			ImGui::TextColoredUnformatted({ 1, 0, 0, 1 }, "Unable to find \"+hostport\"");
		else
			ImGui::TextColored({ 0, 1, 0, 1 }, "+hostport %u", args.m_RCONPort.value());
	}
}

auto TF2CommandLinePage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	m_Data.TryUpdateCmdlineArgs();

	if (!m_Data.m_CommandLineArgs.has_value())
	{
		m_Data.m_TestRCONClient.reset();
		ImGui::TextUnformatted("TF2 must be launched via TF2 Bot Detector. You can open it by clicking the button below.");
		DrawLaunchTF2Button(ds);
	}
	else if (m_Data.m_MultipleInstances)
	{
		m_Data.m_TestRCONClient.reset();
		ImGui::TextUnformatted("More than one instance of hl2.exe found. Please close the other instances.");

		ImGui::EnabledSwitch(false, [&] { DrawLaunchTF2Button(ds); }, "TF2 is currently running. Please close it first.");
	}
	else if (auto& args = m_Data.m_CommandLineArgs.value(); !args.IsPopulated())
	{
		DrawCommandLineArgsInvalid(ds, args);
	}
	else if (!m_Data.m_RCONSuccess)
	{
		auto& args = m_Data.m_CommandLineArgs.value();
		ImGui::TextUnformatted("Connecting to TF2 on 127.0.0.1:"s << args.m_RCONPort.value()
			<< " with password " << std::quoted(args.m_RCONPassword) << "...");

		if (!m_Data.m_TestRCONClient)
			m_Data.m_TestRCONClient.emplace(args.m_RCONPassword, args.m_RCONPort.value());

		ImGui::NewLine();
		m_Data.m_RCONSuccess = m_Data.m_TestRCONClient.value().Update();

		ImGui::NewLine();
		DrawAutoLaunchTF2Checkbox(ds);
	}
	else
	{
		return OnDrawResult::EndDrawing;
	}

	return OnDrawResult::ContinueDrawing;
}

void TF2CommandLinePage::Init(const Settings& settings)
{
	m_Data = {};
	m_Data.m_RandomRCONPassword = GenerateRandomRCONPassword();
	m_Data.m_RandomRCONPort = GenerateRandomRCONPort();
}

void TF2CommandLinePage::Commit(Settings& settings)
{
	m_IsAutoLaunchAllowed = false;
	settings.m_Unsaved.m_RCONClient = std::move(m_Data.m_TestRCONClient.value().m_Client);
	m_Data.m_TestRCONClient.reset();
}
