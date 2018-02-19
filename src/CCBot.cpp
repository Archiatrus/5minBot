#include "sc2api/sc2_api.h"

#include "CCBot.h"
#include "Util.h"
#include "AutoObserver\CameraModule.h"

int lvl85 = 0;
int lvl1000 = 0;
int lvl10000 = 0;
double maxStepTime = -1.0;

CCBot::CCBot()
    : m_map(*this)
    , m_bases(*this)
    , m_unitInfo(*this)
    , m_workers(*this)
    , m_gameCommander(*this)
    , m_strategy(*this)
    , m_techTree(*this)
	, m_cameraModule(this)
{
    
}

void CCBot::OnGameStart() 
{
    m_config.readConfigFile();
	
	// get my race
    auto playerID = Observation()->GetPlayerID();
    for (const auto & playerInfo : Observation()->GetGameInfo().player_info)
    {
        if (playerInfo.player_id == playerID)
        {
            m_playerRace[Players::Self] = playerInfo.race_actual;
        }
        else
        {
            m_playerRace[Players::Enemy] = playerInfo.race_requested;
        }
    }
    m_techTree.onStart();
    m_strategy.onStart();
    m_map.onStart();
    m_unitInfo.onStart();
    m_bases.onStart();
    m_workers.onStart();

    m_gameCommander.onStart();
	if (useAutoObserver)
	{
		m_cameraModule.onStart();
	}
	Actions()->SendChat("5minBot");
	m_time.push_back({ 0.0,0.0,0.0,0.0 });
}

void CCBot::OnStep()
{
	Timer t;
	t.start();
	Control()->GetObservation();

    m_map.onFrame();
    m_unitInfo.onFrame();
    m_bases.onFrame();
    m_workers.onFrame();
    m_strategy.onFrame();
    m_gameCommander.onFrame();

	if (useAutoObserver)
	{
		m_cameraModule.onFrame();
	}

	//OnStep,OnUnitCreated,OnBuildingConstructionComplete,OnUnitEnterVision
	m_time.push_back({ 0.0,0.0,0.0,0.0 });
	m_time[Observation()->GetGameLoop()][0] = t.getElapsedTimeInMilliSec();
	double ms=0.0;
	if (Observation()->GetGameLoop() > 1)
	{
		for (const auto k : m_time[Observation()->GetGameLoop() - 1])
		{
			ms += k;
		}
	}
	if (maxStepTime < ms)
	{
		maxStepTime = ms;
	}
	if (ms > 85.0)
	{
		++lvl85;
		if (ms > 1000.0)
		{
			++lvl1000;
			if (ms > 10000.0)
			{
				++lvl10000;
			}
		}
	}
	Drawing::drawTextScreen(*this, sc2::Point2D(0.85f, 0.6f), "Step time: " + std::to_string(int(std::round(ms))) + "ms\nMax step time: " + std::to_string(int(std::round(maxStepTime))) + "ms\n" + "#Frames >    85ms: " + std::to_string(lvl85) + "\n#Frames >  1000ms: " + std::to_string(lvl1000) + "\n#Frames > 10000ms: " + std::to_string(lvl10000), sc2::Colors::White, 16);
	//std::cout << "#Frames > 85: " << lvl85 << ",    #Frames > 1000: " << lvl1000 << ",    #Frames > 10000ms: " << lvl10000 << std::endl;
	//if (Observation()->GetGameLoop() == 100)
	//{
	//	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR, Bases().getNextExpansion(Players::Self), 2, 1);
	//}

	if (!useDebug)
	{
		return;
	}
	Debug()->SendDebug();
}

void CCBot::OnUnitCreated(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().OnUnitCreate(unit);
	m_gameCommander.onUnitCreate(cunit);
	if (useAutoObserver)
	{
		m_cameraModule.moveCameraUnitCreated(unit);
	}
	m_time[Observation()->GetGameLoop()][1] = t.getElapsedTimeInMilliSec();
}

void CCBot::OnBuildingConstructionComplete(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().getUnit(unit->tag);
	m_gameCommander.OnBuildingConstructionComplete(cunit);
	m_time[Observation()->GetGameLoop()][2] = t.getElapsedTimeInMilliSec();
}


void CCBot::OnUnitEnterVision(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().getUnit(unit->tag);
	m_gameCommander.OnUnitEnterVision(cunit);
	m_time[Observation()->GetGameLoop()][3] = t.getElapsedTimeInMilliSec();
}

void CCBot::OnDTdetected(const sc2::Point2D pos)
{
	m_gameCommander.OnDTdetected(pos);
}

void CCBot::OnUpgradeCompleted(sc2::UpgradeID upgrade)
{
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3)
	{
		m_armor = 3;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2)
	{
		m_armor = 2;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1)
	{
		m_armor = 1;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3)
	{
		m_weapon = 3;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2)
	{
		m_weapon = 2;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1)
	{
		m_weapon = 1;
	}
}
// TODO: Figure out my race
const sc2::Race & CCBot::GetPlayerRace(int player) const
{
    BOT_ASSERT(player == Players::Self || player == Players::Enemy, "invalid player for GetPlayerRace");
    return m_playerRace[player];
}

BotConfig & CCBot::Config()
{
     return m_config;
}

const MapTools & CCBot::Map() const
{
    return m_map;
}

const StrategyManager & CCBot::Strategy() const
{
    return m_strategy;
}

const BaseLocationManager & CCBot::Bases() const
{
    return m_bases;
}

UnitInfoManager & CCBot::UnitInfo()
{
    return m_unitInfo;
}

const TypeData & CCBot::Data(const sc2::UnitTypeID & type) const
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const sc2::UpgradeID & type) const
{
    return m_techTree.getData(type);
}

const TypeData & CCBot::Data(const BuildType & type) const
{
    return m_techTree.getData(type);
}

WorkerManager & CCBot::Workers()
{
    return m_workers;
}

const CUnit_ptr CCBot::GetUnit(const UnitTag & tag)
{
    return UnitInfo().getUnit(tag);
}

sc2::Point2D CCBot::GetStartLocation() const
{
    return Observation()->GetStartLocation();
}

void CCBot::OnError(const std::vector<sc2::ClientError> & client_errors, const std::vector<std::string> & protocol_errors)
{
    
}

const ProductionManager & CCBot::Production()
{
	return m_gameCommander.Production();
}

void CCBot::requestGuards(const bool req)
{
	m_gameCommander.requestGuards(req);
}

std::shared_ptr<shuttle> CCBot::requestShuttleService(const CUnits passengers, const sc2::Point2D targetPos)
{
	return m_gameCommander.requestShuttleService(passengers,targetPos);
}

const int CCBot::getArmor() const
{
	return m_armor;
}

const int CCBot::getWeapon() const
{
	return m_weapon;
}

void * CreateNewAgent()
{
	return new CCBot();
}

const char *GetAgentName()
{
	return "5minBot";
}

int GetAgentRace()
{
	return sc2::Race::Terran;
}