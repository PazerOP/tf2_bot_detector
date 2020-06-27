#pragma once
#ifdef _WIN32

#include <list>
#include <unordered_map>

namespace tf2_bot_detector
{
	class Settings;

	class HijackActionManager final
	{
	public:
		HijackActionManager(const Settings& settings);

	private:
		const Settings* m_Settings = nullptr;

		struct Writer;

		auto absolute_root() const;
		auto absolute_cfg() const;
		auto absolute_cfg_temp() const;

		// Map of temp cfg file content hashes to cfg file name so we don't
		// create a ton of duplicate files.
		std::unordered_multimap<size_t, uint32_t> m_TempCfgFiles;
		uint32_t m_LastUpdateIndex = 0;
		bool ProcessSimpleCommands(const Writer& writer);
		bool ProcessComplexCommands(const Writer& writer);

		bool SendHijackCommands(const Writer& writer);
		bool SendHijackCommand(std::string cmd);

		struct RunningCommand;
		std::list<RunningCommand> m_RunningCommands;
		void ProcessRunningCommands();
	};
}
#endif
