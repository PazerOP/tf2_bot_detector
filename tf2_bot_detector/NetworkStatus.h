#pragma once

#include "IConsoleLine.h"

#include <string>
#include <string_view>

namespace tf2_bot_detector
{
	enum class SocketType : uint8_t
	{
		Client,
		Server,
		HLTV,
		Matchmaking,
		SystemLink,
		LAN,

		COUNT,
	};

	struct SplitPacket
	{
		SplitPacket() = default;
		SplitPacket(SocketType socketType, uint8_t index, uint8_t count, uint16_t sequence, uint16_t size,
			uint16_t mtu, std::string addr, uint16_t totalSize) :
			m_SocketType(socketType), m_Index(index), m_Count(count), m_Sequence(sequence), m_Size(size),
			m_MTU(mtu), m_Address(std::move(addr)), m_TotalSize(totalSize)
		{
		}

		SocketType m_SocketType{};
		uint8_t m_Index{};
		uint8_t m_Count{};
		uint16_t m_Sequence{};
		uint16_t m_Size{};
		uint16_t m_MTU{};
		uint16_t m_TotalSize{};
		std::string m_Address;
	};

	class SplitPacketLine final : public ConsoleLineBase<SplitPacketLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		SplitPacketLine(time_point_t timestamp, SplitPacket packet);

		static std::unique_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const SplitPacket& GetSplitPacket() const { return m_Packet; }

		ConsoleLineType GetType() const override { return ConsoleLineType::SplitPacket; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		SplitPacket m_Packet;
	};
}
