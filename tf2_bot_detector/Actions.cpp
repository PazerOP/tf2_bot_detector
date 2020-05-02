#include "Actions.h"

#include <mh/text/string_insertion.hpp>

#include <iomanip>
#include <ostream>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;

KickAction::KickAction(uint16_t userID, KickReason reason) :
	m_UserID(userID), m_Reason(reason)
{
}

duration_t KickAction::GetMinInterval() const
{
	return 30s;
}

void KickAction::WriteCommands(std::ostream& os) const
{
	const char* reasonString = [&]()
	{
		switch (m_Reason)
		{
		case KickReason::Other:     return "other";
		case KickReason::Cheating:  return "cheating";
		case KickReason::Idle:      return "idle";
		case KickReason::Scamming:  return "scamming";
		default: throw std::runtime_error("Unknown/invalid reason");
		}
	}();

	os << "callvote kick \"" << m_UserID << ' ' << reasonString << "\"\n";
}

GenericCommandAction::GenericCommandAction(std::string cmd) :
	m_Command(std::move(cmd))
{
}

void GenericCommandAction::WriteCommands(std::ostream& os) const
{
	os << m_Command << '\n';
}

ChatMessageAction::ChatMessageAction(const std::string_view& message) :
	//GenericCommandAction("say "s << std::quoted(message))
	GenericCommandAction("echo "s << std::quoted("[TFBD_DEBUG_CHAT_MSG] "s << message))
{
}

duration_t ChatMessageAction::GetMinInterval() const
{
	return 5s;
}
