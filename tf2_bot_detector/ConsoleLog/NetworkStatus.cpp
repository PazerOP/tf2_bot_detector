#include "NetworkStatus.h"
#include "Util/RegexUtils.h"
#include "Log.h"

#include <imgui.h>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

// net_status
static std::regex s_DataPerClientOut(R"regex(           per client out (\d+\.\d), in (\d+\.\d) kB\/s)regex", std::regex::optimize);

SplitPacketLine::SplitPacketLine(time_point_t timestamp, SplitPacket packet) :
	BaseClass(timestamp), m_Packet(std::move(packet))
{
}

std::shared_ptr<IConsoleLine> SplitPacketLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static std::regex s_Regex(R"regex(<-- \[(.{3})\] Split packet +(\d+)\/ +(\d+) seq +(\d+) size +(\d+) mtu +(\d+) from ([0-9.:a-fA-F]+:\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		SplitPacket packet;

		{
			auto socket = to_string_view(result[1]);
			if (socket == "cl "sv)
				packet.m_SocketType = SocketType::Client;
			else if (socket == "sv "sv)
				packet.m_SocketType = SocketType::Server;
			else if (socket == "htv"sv)
				packet.m_SocketType = SocketType::HLTV;
			else if (socket == "mat"sv)
				packet.m_SocketType = SocketType::Matchmaking;
			else if (socket == "lnk"sv)
				packet.m_SocketType = SocketType::SystemLink;
			else if (socket == "lan"sv)
				packet.m_SocketType = SocketType::LAN;
			else
				throw std::runtime_error("Unknown socket type "s << std::quoted(socket));
		}

		from_chars_throw(result[2], packet.m_Index);
		assert(packet.m_Index > 0);
		if (packet.m_Index > 0)
			packet.m_Index--;

		from_chars_throw(result[3], packet.m_Count);
		from_chars_throw(result[4], packet.m_Sequence);
		from_chars_throw(result[5], packet.m_Size);
		from_chars_throw(result[6], packet.m_MTU);
		packet.m_Address = result[7].str();

		return std::make_shared<SplitPacketLine>(timestamp, std::move(packet));
	}

	return nullptr;
}

void SplitPacketLine::Print(const PrintArgs& args) const
{
	const char* socketType = "???";
	switch (m_Packet.m_SocketType)
	{
	case SocketType::Client:       socketType = "cl "; break;
	case SocketType::Server:       socketType = "sv "; break;
	case SocketType::HLTV:         socketType = "htv"; break;
	case SocketType::Matchmaking:  socketType = "mat"; break;
	case SocketType::SystemLink:   socketType = "lnk"; break;
	case SocketType::LAN:          socketType = "lan"; break;

	default:
		throw std::runtime_error("Invalid SocketType");
	}

	ImGui::Text("--> [%s] Split packet %4i/%4i seq %5i size %4i mtu %4i to %s [ total %4i ]",
		socketType,
		m_Packet.m_Index + 1, m_Packet.m_Count,
		m_Packet.m_Sequence,
		m_Packet.m_Size,
		m_Packet.m_MTU,
		m_Packet.m_Address.c_str(),
		m_Packet.m_TotalSize);
}

NetStatusConfigLine::NetStatusConfigLine(time_point_t timestamp,
	PlayerMode playerMode, ServerMode serverMode, unsigned connectionCount) :
	BaseClass(timestamp), m_PlayerMode(playerMode), m_ServerMode(serverMode), m_ConnectionCount(connectionCount)
{
}

std::shared_ptr<IConsoleLine> NetStatusConfigLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(- Config: (.*), (.*), (\d+) connections)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		const std::string_view playerModeStr(&*result[1].first, result[1].length());
		PlayerMode playerMode;
		if (playerModeStr == "Multiplayer"sv)
			playerMode = PlayerMode::Multiplayer;
		else if (playerModeStr == "Singleplayer"sv)
			playerMode = PlayerMode::Singleplayer;
		else
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown player mode "s << std::quoted(playerModeStr));
			return nullptr;
		}

		const std::string_view serverModeStr(&*result[2].first, result[2].length());
		ServerMode serverMode;
		if (serverModeStr == "dedicated"sv)
			serverMode = ServerMode::Dedicated;
		else if (serverModeStr == "listen"sv)
			serverMode = ServerMode::Listen;
		else
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown server mode "s << std::quoted(serverModeStr));
			return nullptr;
		}

		unsigned connectionCount;
		from_chars_throw(result[3], connectionCount);

		return std::make_shared<NetStatusConfigLine>(timestamp, playerMode, serverMode, connectionCount);
	}

	return nullptr;
}

void NetStatusConfigLine::Print(const PrintArgs& args) const
{
	ImGui::Text("- Config: %s, %s, %u connections",
		m_PlayerMode == PlayerMode::Multiplayer ? "Multiplayer" : "Singleplayer",
		m_ServerMode == ServerMode::Dedicated ? "dedicated" : "listen",
		m_ConnectionCount);
}
