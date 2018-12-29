#pragma once
#include <string>
#include <map>

#include "sc2api/sc2_api.h"
#include "Common.h"

// Only add builds at the end of the list of builds. Otherwise the 'learned' data is invalid.
constexpr auto BUILDS_START_LINE = __LINE__;
enum class build
{
	Bio0Rax,
	Bio1Rax,
	Bio2Rax,
	BioParanoid
};
constexpr auto NO_BUILDS = __LINE__ - BUILDS_START_LINE - 4;

class BotConfig
{
 public:
	size_t m_buildingSpacing;

	BotConfig();
	void onStart();
	build getBuild() const;
	void processResult(const sc2::GameResult result);
	void setOpponentID(const std::string& opponentID);
	void setOpponentRace(const std::string& opponentRace);

private:

	std::string m_opponentID;
	std::string m_opponentRace;
	size_t m_noBuilds;
	build m_chosenBuild;
	std::map<std::string, std::vector<int>> m_buildStats;

	void readConfigFile();
	void decideForBuild();
};
