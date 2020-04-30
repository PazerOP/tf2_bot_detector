#include "ActionManager.h"
#include "Actions.h"
#include "ConsoleLines.h"

#include <mh/text/string_insertion.hpp>

#include <filesystem>

using namespace tf2_bot_detector;
using namespace std::string_view_literals;

static const std::filesystem::path s_UpdateCFGPath("C:/Program Files (x86)/Steam/steamapps/common/Team Fortress 2/tf/custom/tf2_bot_detector/cfg/tf2_bot_detector/temp/update.cfg");

ActionManager::~ActionManager()
{
}

void ActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
	m_Actions.emplace(++m_LastUpdateIndex, std::move(action));
}

void ActionManager::Update()
{
	if (m_Actions.empty())
		return;

	const auto now = clock_t::now();
	if (now < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	struct FileDeleter
	{
		void operator()(FILE* f) const { fclose(f); }
	};

	std::unique_ptr<FILE, FileDeleter> file(_wfsopen(s_UpdateCFGPath.c_str(), L"w", _SH_DENYRW));
	if (!file)
		return;

	std::string fileContents;
	mh::strwrapperstream stream(fileContents);

	for (const auto& action : m_Actions)
	{
		action.second->WriteCommands(stream);
		stream << "tfbd_update_count " << action.first << '\n';
		stream << "cvarlist tfbd_update_count\n";
	}

	const auto count = fwrite(fileContents.data(), fileContents.size(), 1, file.get());
	assert(count == 1);
	if (count != 1)
		throw std::runtime_error("Failed to write update.cfg, even though we should have already guarenteed exclusive access");

	m_LastUpdateTime = now;
}

void ActionManager::OnConsoleLineParsed(IConsoleLine& line)
{
	if (line.GetType() == ConsoleLineType::CvarlistConvar)
	{
		const auto& cvar = static_cast<const CvarlistConvarLine&>(line);
		if (cvar.GetConvarName() == "tfbd_update_count"sv)
		{
			const auto value = static_cast<uint32_t>(cvar.GetConvarValue());
			const auto upperBound = m_Actions.upper_bound(value);
			m_Actions.erase(m_Actions.begin(), upperBound);
		}
	}
}
