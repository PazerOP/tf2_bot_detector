#include "Actions.h"

#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>

#include <iomanip>
#include <ostream>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

KickAction::KickAction(uint16_t userID, KickReason reason) :
	GenericCommandAction(MakeCommand(userID, reason))
{
}

duration_t KickAction::GetMinInterval() const
{
	return 30s;
}

std::string KickAction::MakeCommand(uint16_t userID, KickReason reason)
{
	const char* reasonString = [&]()
	{
		switch (reason)
		{
		case KickReason::Other:     return "other";
		case KickReason::Cheating:  return "cheating";
		case KickReason::Idle:      return "idle";
		case KickReason::Scamming:  return "scamming";
		default: throw std::runtime_error("Unknown/invalid reason");
		}
	}();

	return "callvote kick \""s << userID << ' ' << reasonString << '"';
}

GenericCommandAction::GenericCommandAction(std::string cmd) :
	m_Command(std::move(cmd))
{
}

void GenericCommandAction::WriteCommands(std::ostream& os) const
{
	auto cleaned = mh::find_and_replace(m_Command, "\"", "'");
	os << "echo "s << std::quoted("[TFBD_DEBUG_CMD] "s << cleaned) << '\n';
}

ChatMessageAction::ChatMessageAction(const std::string_view& message) :
	GenericCommandAction("say "s << std::quoted(message))
{
}

duration_t ChatMessageAction::GetMinInterval() const
{
	return 5s;
}
