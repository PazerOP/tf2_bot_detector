#include "PlayerList.h"
#include "SteamID.h"

#include <fstream>
#include <string>

using namespace tf2_bot_detector;
namespace fs = std::filesystem;

PlayerList::PlayerList(const fs::path& filename)
{
	LoadFile(filename);
}

void PlayerList::LoadFile(const fs::path& filename)
{
	std::ifstream file(filename);

	m_Players.clear();

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
			continue;

		IncludePlayer(SteamID(line));
	}
}

void PlayerList::SaveFile(const fs::path& filename)
{
	struct FileDeleter
	{
		void operator()(FILE* f) const { fclose(f); }
	};

	char tmpFileName[L_tmpnam]{};
	if (auto err = tmpnam_s(tmpFileName))
		throw std::runtime_error("Failed to create temporary file for cheater list");

	{
		std::ofstream tmpFile(tmpFileName);

		for (const auto& cheater : m_Players)
			tmpFile << cheater << '\n';
	}

	fs::copy(tmpFileName, filename, fs::copy_options::overwrite_existing);

	fs::remove(tmpFileName);
}

void PlayerList::IncludePlayer(const SteamID& id, bool included)
{
	if (included)
		m_Players.insert(id);
	else
		m_Players.erase(id);
}

bool PlayerList::IsPlayerIncluded(const SteamID& id) const
{
	return m_Players.find(id) != m_Players.end();
}
