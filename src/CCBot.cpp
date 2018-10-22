#include "sc2api/sc2_api.h"

#include "CCBot.h"
#include "Util.h"

int lvl85 = 0;
int lvl1000 = 0;
int lvl10000 = 0;
double maxStepTime = -1.0;

#ifdef VSHUMAN
Timer timer;
double oldTime=0.0;
#endif // VSHUMAN


CCBot::CCBot()
	: m_map(*this)
	, m_bases(*this)
	, m_unitInfo(*this)
    , m_workers(*this)
    , m_strategy(*this)
	, m_techTree(*this)
    , m_gameCommander(*this)
	, m_cameraModule(this)
	, m_armorBio(0)
	, m_weaponsBio(0)
	, m_weaponsMech(0)
{
	
}

void CCBot::OnGameStart() 
{
	#ifdef VSHUMAN
	timer.start();
	#endif // VSHUMAN
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
	// Actions()->SendChat("5minBot");
}

void CCBot::OnStep()
{
	/*
	const auto actionTags = Actions()->Commands();
	Drawing::cout{} << "number of tags: " << actionTags.size() << std::endl;
	if (actionTags.size() > 30)
	{
		for (const auto & tag : actionTags)
		{
			const auto unit = GetUnit(tag);
			if (unit)
			{
				auto order = unit->getOrders();
				Drawing::cout{} << "Unit: " << Observation()->GetUnitTypeData()[unit->getUnitType()].name;
				if (!order.empty())
				{
					Drawing::cout{} << ", Ability: " << Observation()->GetAbilityData()[order.front().ability_id].friendly_name;
				}
				Drawing::cout{} << std::endl;
			}
		}
	}
	*/
	// if (Observation()->GetGameLoop() == 10)
	// {
	//  	Debug()->DebugCreateUnit(sc2::UNIT_TYPEID::ZERG_HYDRALISK, Bases().getNewestExpansion(Players::Self), Players::Self, 2);
	// }
	Timer t;
	t.start();
	// Control()->GetObservation();

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

	// OnStep,OnUnitCreated,OnBuildingConstructionComplete,OnUnitEnterVision
	m_time[Observation()->GetGameLoop()][0] = t.getElapsedTimeInMilliSec();
	double ms=0.0;
	if (Observation()->GetGameLoop() > 1)
	{
		for (const auto k : m_time[Observation()->GetGameLoop() - 1])
		{
			ms += k.second;
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
	Drawing::drawTextScreen(*this, sc2::Point2D(0.85f, 0.6f), "Step time: " + std::to_string(int(std::round(ms))) + "ms\nMax step time: " + std::to_string(int(std::round(maxStepTime))) + "ms\n" + "#Frames >	85ms: " + std::to_string(lvl85) + "\n#Frames >  1000ms: " + std::to_string(lvl1000) + "\n#Frames > 10000ms: " + std::to_string(lvl10000), sc2::Colors::White, 16);

	if (useDebug)
	{
		Debug()->SendDebug();
	}
	#ifdef VSHUMAN
	while (timer.getElapsedTimeInMilliSec() - oldTime < 1000.0 / 22.4) {}
	oldTime = timer.getElapsedTimeInMilliSec();
	#endif // VSHUMAN
}

void CCBot::OnUnitCreated(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().OnUnitCreate(unit);
	if (cunit)
	{
		m_gameCommander.onUnitCreate(cunit);
		if (useAutoObserver)
		{
			m_cameraModule.moveCameraUnitCreated(unit);
		}
	}
	m_time[Observation()->GetGameLoop()][1] = t.getElapsedTimeInMilliSec();
}

void CCBot::OnBuildingConstructionComplete(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().getUnit(unit->tag);
	if (cunit)
	{
		m_gameCommander.OnBuildingConstructionComplete(cunit);
	}
	m_time[Observation()->GetGameLoop()][2] = t.getElapsedTimeInMilliSec();
}


void CCBot::OnUnitEnterVision(const sc2::Unit * unit)
{
	Timer t;
	t.start();
	const CUnit_ptr cunit = UnitInfo().OnUnitCreate(unit);
	if (cunit)
	{
		m_gameCommander.OnUnitEnterVision(cunit);
	}
	m_time[Observation()->GetGameLoop()][3] = t.getElapsedTimeInMilliSec();
}

void CCBot::OnUnitDestroyed(const sc2::Unit * unit)
{
	m_gameCommander.onUnitDestroyed(unit);
}

void CCBot::OnCloakDetected(const sc2::UNIT_TYPEID & type, const sc2::Point2D & pos)
{
	m_gameCommander.OnDetectedNeeded(type, pos);
}

void CCBot::OnUpgradeCompleted(sc2::UpgradeID upgrade)
{
	bool shouldWeAttack = (GetPlayerRace(Players::Enemy) == sc2::Race::Zerg && false)
		|| (GetPlayerRace(Players::Enemy) == sc2::Race::Protoss && Bases().getOccupiedBaseLocations(Players::Enemy).size() > Bases().getOccupiedBaseLocations(Players::Self).size())
		|| (GetPlayerRace(Players::Enemy) == sc2::Race::Terran && Bases().getOccupiedBaseLocations(Players::Enemy).size() > Bases().getOccupiedBaseLocations(Players::Self).size());
	Drawing::cout{} << "shouldWeAttack " << shouldWeAttack << " (" << Bases().getOccupiedBaseLocations(Players::Enemy).size() << " > " << Bases().getOccupiedBaseLocations(Players::Self).size() << ")" << std::endl;
	if (upgrade == sc2::UPGRADE_ID::SHIELDWALL && !underAttack())
	{
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}

	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3)
	{
		m_armorBio = 3;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2)
	{
		m_armorBio = 2;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1)
	{
		m_armorBio = 1;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3)
	{
		m_weaponsBio = 3;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2)
	{
		m_weaponsBio = 2;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1)
	{
		m_weaponsBio = 1;
		if (shouldWeAttack)
		{
			m_gameCommander.attack(true);
			Drawing::cout{} << "Attacking " << shouldWeAttack << std::endl;
		}
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3)
	{
		m_weaponsMech = 3;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2)
	{
		m_weaponsMech = 2;
	}
	if (upgrade == sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1)
	{
		m_weaponsMech = 1;
	}
}

void CCBot::retreat()
{
	m_gameCommander.attack(false);
}

const sc2::Race & CCBot::GetPlayerRace(const int player) const
{
	BOT_ASSERT(player == Players::Self || player == Players::Enemy, "invalid player for GetPlayerRace");
	return m_playerRace.at(player);
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

int CCBot::Data(const sc2::AbilityID & type) const
{
	return m_techTree.getData(type);
}

WorkerManager & CCBot::Workers()
{
	return m_workers;
}

const CUnit_ptr CCBot::GetUnit(const UnitTag & tag)
{
	if (tag == 0)
	{
		return nullptr;
	}
	return UnitInfo().getUnit(tag);
}

sc2::Point2D CCBot::GetStartLocation() const
{
	return Observation()->GetStartLocation();
}


ProductionManager & CCBot::Production()
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

int CCBot::getArmorBio() const
{
	return m_armorBio;
}

int CCBot::getWeaponBio() const
{
	return m_weaponsBio;
}

int CCBot::getWeaponMech() const
{
	return m_weaponsMech;
}

bool CCBot::underAttack() const
{
	return m_gameCommander.underAttack();
}

void CCBot::setPlayerRace(int player, sc2::Race race)
{
	m_playerRace.at(player) = race;
}

std::string ClientErrorToString(sc2::ClientError error)
{
	switch (error)
	{
	case(sc2::ClientError::ErrorSC2): return "ErrorSC2";
	case(sc2::ClientError::InvalidAbilityRemap): return "InvalidAbilityRemap"; /*! An ability was improperly mapped to an ability id that doesn't exist. */
	case(sc2::ClientError::InvalidResponse): return "InvalidResponse";     /*! The response does not contain a field that was expected. */
	case(sc2::ClientError::NoAbilitiesForTag): return "NoAbilitiesForTag";   /*! The unit does not have any abilities. */
	case(sc2::ClientError::ResponseNotConsumed): return "ResponseNotConsumed"; /*! A request was made without consuming the response from the previous request, that puts this library in an illegal state. */
	case(sc2::ClientError::ResponseMismatch): return "ResponseMismatch";    /*! The response received from SC2 does not match the request. */
	case(sc2::ClientError::ConnectionClosed): return "ConnectionClosed";    /*! The websocket connection has prematurely closed, this could mean starcraft crashed or a websocket timeout has occurred. */
	case(sc2::ClientError::SC2UnknownStatus): return "SC2UnknownStatus";
	case(sc2::ClientError::SC2AppFailure): return "SC2AppFailure";       /*! SC2 has either crashed or been forcibly terminated by this library because it was not responding to requests. */
	case(sc2::ClientError::SC2ProtocolError): return "SC2ProtocolError";    /*! The response from SC2 contains errors, most likely meaning the API was not used in a correct way. */
	case(sc2::ClientError::SC2ProtocolTimeout): return "SC2ProtocolTimeout";  /*! A request was made and a response was not received in the amount of time given by the timeout. */
	case(sc2::ClientError::WrongGameVersion): return "WrongGameVersion";
	}
	return "Unknown ClientError";
}


void CCBot::OnError(const std::vector<sc2::ClientError> & client_errors, const std::vector<std::string> & protocol_errors)
{
	for (const auto & error : protocol_errors)
	{
		std::cerr << "Protocol error: " << error << std::endl;
	}
	for (const auto & error : client_errors)
	{
		std::cerr << "Client error: " << ClientErrorToString(error) << std::endl;
	}

}
