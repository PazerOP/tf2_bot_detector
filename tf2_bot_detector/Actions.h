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
		LobbyUpdate,

		COUNT,
	};

	class IAction
	{
	public:
		virtual ~IAction() = default;

		virtual duration_t GetMinInterval() const { return {}; }
		virtual ActionType GetType() const = 0;
		virtual size_t GetMaxQueuedCount() const { return size_t(-1); }
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

	class LobbyUpdateAction : public IAction
	{
	public:
		ActionType GetType() const override { return ActionType::LobbyUpdate; }
		duration_t GetMinInterval() const override;
		size_t GetMaxQueuedCount() const { return 1; }
		void WriteCommands(std::ostream& os) const override;
	};

	enum class KickReason
	{
		Other,
		Cheating,
		Idle,
		Scamming,
	};

	class KickAction final : public GenericCommandAction
	{
	public:
		KickAction(uint16_t userID, KickReason reason);

		duration_t GetMinInterval() const override;
		ActionType GetType() const override { return ActionType::Kick; }

	private:
		static std::string MakeCommand(uint16_t userID, KickReason reason);
	};

	enum class ChatMessageType
	{
		Public,
		Team,
		Party,
	};

	class ChatMessageAction final : public GenericCommandAction
	{
	public:
		ChatMessageAction(const std::string_view& message, ChatMessageType type = ChatMessageType::Public);

		duration_t GetMinInterval() const override;
		ActionType GetType() const override { return ActionType::ChatMessage; }

	private:
		static std::string_view GetCommand(ChatMessageType type);
		static std::string ScrubMessage(const std::string_view& msg);
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
