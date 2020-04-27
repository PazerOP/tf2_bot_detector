#include "CheaterList.h"
#include "SteamID.h"

#include <fstream>
#include <string>

using namespace tf2_bot_detector;
namespace fs = std::filesystem;

CheaterList::CheaterList(const fs::path& filename)
{
	LoadFile(filename);
}

void CheaterList::LoadFile(const fs::path& filename)
{
	std::ifstream file(filename);

	m_Cheaters.clear();

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
			continue;

		SetIsCheater(SteamID(line));
	}
}

void CheaterList::SaveFile(const fs::path& filename)
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

		for (const auto& cheater : m_Cheaters)
			tmpFile << cheater << '\n';
	}

	fs::copy(tmpFileName, filename, fs::copy_options::overwrite_existing);

	fs::remove(tmpFileName);
}

void CheaterList::SetIsCheater(const SteamID& id, bool isCheater)
{
	if (isCheater)
		m_Cheaters.insert(id);
	else
		m_Cheaters.erase(id);
}

bool CheaterList::IsCheater(const SteamID& id) const
{
	return m_Cheaters.find(id) != m_Cheaters.end();
}
