#pragma once

#include "Clock.h"

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace tf2_bot_detector
{
	enum class ActionType
	{
		GenericCommand,
		Kick,
		ChatMessage,

		COUNT,
	};

	class IAction
	{
	public:
		virtual ~IAction() = default;

		virtual duration_t GetMinInterval() const { return {}; }
		virtual ActionType GetType() const = 0;
		virtual void WriteCommands(std::ostream& os) const {}
	};

	class GenericCommandAction : public IAction
	{
	public:
		GenericCommandAction(std::string cmd);

		ActionType GetType() const override { return ActionType::GenericCommand; }
		void WriteCommands(std::ostream& os) const;

	private:
		std::string m_Command;
	};

	enum class KickReason
	{
		Other,
		Cheating,
		Idle,
		Scamming,
	};

	class KickAction final : public IAction
	{
	public:
		KickAction(uint16_t userID, KickReason reason);

		duration_t GetMinInterval() const override;
		ActionType GetType() const override { return ActionType::Kick; }
		void WriteCommands(std::ostream& os) const override;

		uint16_t GetUserID() const { return m_UserID; }
		KickReason GetReason() const { return m_Reason; }

	private:
		uint16_t m_UserID;
		KickReason m_Reason;
	};

	class ChatMessageAction final : public GenericCommandAction
	{
	public:
		ChatMessageAction(const std::string_view& message);

		duration_t GetMinInterval() const override;
		ActionType GetType() const override { return ActionType::ChatMessage; }
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::KickReason reason)
{
	using tf2_bot_detector::KickReason;
	switch (reason)
	{
	case KickReason::Cheating: return os << "KickReason::Cheating";
	case KickReason::Idle: return os << "KickReason::Idle";
	case KickReason::Other: return os << "KickReason::Other";
	case KickReason::Scamming: return os << "KickReason::Scamming";

	default:
		assert(!"Unknown KickReason");
		return os << "KickReason(" << std::underlying_type_t<KickReason>(reason) << ')';
	}
}
