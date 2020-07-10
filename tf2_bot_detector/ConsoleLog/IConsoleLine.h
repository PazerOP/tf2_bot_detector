#pragma once

#include "Clock.h"

#include <list>
#include <memory>
#include <string_view>

namespace tf2_bot_detector
{
	enum class ConsoleLineType
	{
		Generic,
		Chat,
		Ping,
		LobbyStatusFailed,
		LobbyChanged,
		LobbyHeader,
		LobbyMember,
		PlayerStatus,
		PlayerStatusShort,
		PlayerStatusCount,
		ClientReachedServerSpawn,
		KillNotification,
		CvarlistConvar,
		VoiceReceive,
		EdictUsage,
		SplitPacket,
		SVC_UserMessage,
		ConfigExec,

		NetLatency,
		NetLoss,
		NetPacketsTotal,
		NetPacketsPerClient,
		NetDataTotal,
		NetDataPerClient,

		NetChannelOnline,
		NetChannelReliable,
		NetChannelLatencyLoss,
		NetChannelPackets,
		NetChannelChoke,
		NetChannelFlow,
		NetChannelTotal,
	};

	enum class ConsoleLineOrdering
	{
		Auto,
		First,
		Last,
	};

	class IConsoleLine : public std::enable_shared_from_this<IConsoleLine>
	{
	public:
		IConsoleLine(time_point_t timestamp);
		virtual ~IConsoleLine() = default;

		virtual ConsoleLineType GetType() const = 0;
		virtual bool ShouldPrint() const { return true; }
		virtual void Print() const = 0;

		static std::shared_ptr<IConsoleLine> ParseConsoleLine(const std::string_view& text, time_point_t timestamp);

		time_point_t GetTimestamp() const { return m_Timestamp; }

	protected:
		using TryParseFunc = std::shared_ptr<IConsoleLine>(*)(const std::string_view& text, time_point_t timestamp);
		struct ConsoleLineTypeData
		{
			TryParseFunc m_TryParseFunc = nullptr;
			const std::type_info* m_TypeInfo = nullptr;

			size_t m_AutoParseSuccessCount = 0;
			bool m_AutoParse = true;
		};

		//static const ConsoleLineTypeData* GetTypeData() { return s_TypeData; }
		static void AddTypeData(ConsoleLineTypeData data);

	private:
		time_point_t m_Timestamp;

		static std::list<ConsoleLineTypeData>& GetTypeData();
		inline static ConsoleLineTypeData* s_TypeData = nullptr;
		inline static size_t s_TotalParseCount = 0;
	};

	template<typename TSelf, bool AutoParse = true>
	class ConsoleLineBase : public IConsoleLine
	{
	public:
		ConsoleLineBase(time_point_t timestamp) : IConsoleLine(timestamp) {}

	private:
		struct AutoRegister
		{
			AutoRegister()
			{
				AddTypeData(ConsoleLineTypeData
					{
						.m_TryParseFunc = &TSelf::TryParse,
						.m_TypeInfo = &typeid(TSelf),
						.m_AutoParse = AutoParse
					});
			}

		} inline static s_AutoRegister;
	};
}
