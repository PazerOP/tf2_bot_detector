#include "PlayerList.h"
#include "SteamID.h"

#include <fstream>
#include <string>

using namespace tf2_bot_detector;
namespace fs = std::filesystem;

PlayerList::PlayerList(fs::path filename) :
	m_Filename(std::move(filename))
{
	LoadFile();
}

void PlayerList::LoadFile()
{
	std::ifstream file(m_Filename);

	m_Players.clear();

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
			continue;

		m_Players.insert(SteamID(line));
	}

	SaveFile(); // Immediately resave so everything is always in the "proper" format
}

void PlayerList::SaveFile()
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

	fs::copy(tmpFileName, m_Filename, fs::copy_options::overwrite_existing);

	fs::remove(tmpFileName);
}

bool PlayerList::IncludePlayer(const SteamID& id, bool included, bool saveFile)
{
	bool modified;
	if (included)
		modified = m_Players.insert(id).second;
	else
		modified = m_Players.erase(id) > 0;

	if (saveFile && modified)
		SaveFile();

	return modified;
}

bool PlayerList::IsPlayerIncluded(const SteamID& id) const
{
	return m_Players.find(id) != m_Players.end();
}
