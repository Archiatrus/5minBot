#include  <random>
#include <iostream>

#include "BotConfig.h"

#include "sc2utils/sc2_manage_process.h"

#include "rapidjson/document.h"
#include "JSONTools.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"


constexpr auto configFile = "Data/config.json";

BotConfig::BotConfig():
	m_opponentID(" "),
	m_opponentRace("Random"),
	m_noBuilds(NO_BUILDS),
	m_chosenBuild(build::Bio0Rax),
	m_buildingSpacing(1)
{

}

void BotConfig::onStart()
{
	readConfigFile();
	decideForBuild();
	processResult(sc2::GameResult::Win);
}

void BotConfig::setOpponentID(const std::string& opponentID)
{
	m_opponentID = opponentID;
}

void BotConfig::setOpponentRace(const std::string& opponentRace)
{
	m_opponentRace = opponentRace;
}

void BotConfig::readConfigFile()
{
	if (sc2::DoesFileExist(configFile))
	{
		const std::string config = JSONTools::ReadFile(configFile);
		if (config.length() > 0)
		{
			rapidjson::Document doc;
			bool parsingFailed = doc.Parse(config.c_str()).HasParseError();
			if (!parsingFailed)
			{
				for (rapidjson::Value::ConstMemberIterator member = doc.MemberBegin(); member != doc.MemberEnd(); ++member)
				{
					if (member->value.IsArray())
					{
						auto opponent = member->name.GetString();
						m_buildStats[opponent].resize(m_noBuilds);
						for (rapidjson::SizeType i(0); i < member->value.Size() && i < m_noBuilds; ++i)
						{
							m_buildStats.at(opponent).at(i) = member->value[i].GetInt();
						}
					}
				}
			}
		}
	}
	if (m_buildStats.empty())
	{
		// if !sc2::DoesFileExist(configFile) || config.length() == 0 || parsingFailed
		m_buildStats["Protoss"].resize(m_noBuilds);
		m_buildStats["Random"].resize(m_noBuilds);
		m_buildStats["Terran"].resize(m_noBuilds);
		m_buildStats["Zerg"].resize(m_noBuilds);
	}
	std::cout << "Memory:" << std::endl;
	for (const auto& a : m_buildStats)
	{
		std::cout << a.first << " : ";
		for (const auto b : a.second)
		{
			std::cout << b << " ";
		}
		std::cout << std::endl;
	}
}


void BotConfig::decideForBuild()
{
	std::vector<int> buildMemory;
	const auto& opponent = m_buildStats.find(m_opponentID);
	if (m_buildStats.end() != opponent)
	{
		buildMemory = opponent->second;
	}
	else
	{
		if (" " != m_opponentID)
		{
			m_buildStats[m_opponentID] = m_buildStats[m_opponentRace];
		}
		buildMemory = m_buildStats[m_opponentRace];
	}
	auto min = *std::min_element(buildMemory.begin(), buildMemory.end());

	// I could do this much(!) smarter... but I am too lazy atm
	if (min <= 0)
	{
		--min;
		for (auto & val : buildMemory)
		{
			val -= min;
		}
	}
	std::vector<int> pool;
	for (auto i(0); i < buildMemory.size(); ++i)
	{
		for (auto ii(0); ii < buildMemory[i]; ++ii)
		{
			pool.push_back(i);
		}
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, pool.size() - 1);
	m_chosenBuild = static_cast<build>(pool[dis(gen)]);
	std::cout << "Picked build : " << static_cast<int>(m_chosenBuild) << std::endl;
}

void BotConfig::processResult(const sc2::GameResult result)
{
	switch (result)
	{
	case sc2::GameResult::Win:
	{
		if (" " != m_opponentID)
		{
			++m_buildStats[m_opponentID][static_cast<int>(m_chosenBuild)];
		}
		++m_buildStats[m_opponentRace][static_cast<int>(m_chosenBuild)];
		break;
	}
	case sc2::GameResult::Loss:
	{
		if (" " != m_opponentID)
		{
			--m_buildStats[m_opponentID][static_cast<int>(m_chosenBuild)];
		}
		--m_buildStats[m_opponentRace][static_cast<int>(m_chosenBuild)];
		break;
	}
	default: break;
	}

	rapidjson::Document ResultsDoc;
	rapidjson::Document::AllocatorType& alloc = ResultsDoc.GetAllocator();
	ResultsDoc.SetObject();
	rapidjson::Value ResultsArray(rapidjson::kArrayType);

	for (const auto& opponentStats : m_buildStats)
	{
		rapidjson::Value ResultsArray(rapidjson::kArrayType);
		for (const auto& val : opponentStats.second)
		{
			ResultsArray.PushBack(rapidjson::Value(val).Move(), alloc);
		}
		rapidjson::Value key((opponentStats.first).c_str(), alloc);
		ResultsDoc.AddMember(key, ResultsArray, alloc);
	}

	std::ofstream ofs(configFile);
	rapidjson::OStreamWrapper osw(ofs);
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
	ResultsDoc.Accept(writer);
}

build BotConfig::getBuild() const
{
	return m_chosenBuild;
}