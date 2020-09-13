#pragma once

#include "Clock.h"
#include "ICommandSource.h"

#include <mh/reflection/enum.hpp>

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
		StatusUpdate,

		COUNT,
	};

	class IAction : public ICommandSource
	{
	public:
		virtual ~IAction() = default;

		virtual duration_t GetMinInterval() const { return {}; }
		virtual ActionType GetType() const = 0;
		virtual size_t GetMaxQueuedCount() const { return size_t(-1); }
	};

	class GenericCommandAction : public IAction
	{
	public:
		explicit GenericCommandAction(std::string cmd, std::string args = std::string());

		ActionType GetType() const override { return ActionType::GenericCommand; }
		void WriteCommands(ICommandWriter& writer) const;

	private:
		std::string m_Command;
		std::string m_Args;
	};

	class LobbyUpdateAction : public IAction
	{
	public:
		ActionType GetType() const override { return ActionType::LobbyUpdate; }
		void WriteCommands(ICommandWriter& writer) const override;
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
		size_t GetMaxQueuedCount() const override { return 1; }

	private:
		static std::string MakeArgs(uint16_t userID, KickReason reason);
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
		size_t GetMaxQueuedCount() const override { return 2; }

	private:
		static std::string_view GetCommand(ChatMessageType type);
		static std::string ScrubMessage(std::string msg);
	};

#if 0
	class StatusUpdateAction final : public GenericCommandAction
	{
	public:
		StatusUpdateAction(bool shortStatus = false);

		duration_t GetMinInterval() const override;
		ActionType GetType() const override { return ActionType::StatusUpdate; }
	};
#endif
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::KickReason)
	MH_ENUM_REFLECT_VALUE(Cheating)
	MH_ENUM_REFLECT_VALUE(Idle)
	MH_ENUM_REFLECT_VALUE(Other)
	MH_ENUM_REFLECT_VALUE(Scamming)
MH_ENUM_REFLECT_END()
