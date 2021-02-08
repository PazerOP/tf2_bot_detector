#pragma once

#include "SteamID.h"

#include <mh/coroutine/task.hpp>
#include <mh/reflection/struct.hpp>

#include <memory>

namespace tf2_bot_detector
{
	class IHTTPClient;
}

namespace tf2_bot_detector::LogsTFAPI
{
	struct PlayerLogsInfo
	{
		SteamID m_ID;
		uint32_t m_LogsCount;
	};

	mh::task<PlayerLogsInfo> GetPlayerLogsInfoAsync(std::shared_ptr<const IHTTPClient> client, SteamID id);
}

MH_STRUCT_REFLECT_BEGIN(tf2_bot_detector::LogsTFAPI::PlayerLogsInfo)
	MH_STRUCT_REFLECT_MEMBER(m_ID);
	MH_STRUCT_REFLECT_MEMBER(m_LogsCount);
MH_STRUCT_REFLECT_END();
