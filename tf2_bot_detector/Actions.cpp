#include "Actions.h"

#include <ostream>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

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
