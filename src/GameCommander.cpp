#include "GameCommander.h"
#include "CCBot.h"
#include "Util.h"

//Only harass if we have more than
const int manyUnits = 25;

timePlace::timePlace(uint32_t time, sc2::Point2D place) : m_time(time), m_place(place)
{

}

GameCommander::GameCommander(CCBot & bot)
    : m_bot                 (bot)
    , m_productionManager   (bot)
    , m_scoutManager        (bot)
	, m_harassManager		(bot)
    , m_combatCommander     (bot)
    , m_initialScoutSet     (false)
{

}

void GameCommander::onStart()
{
    m_productionManager.onStart();
    m_scoutManager.onStart();
	m_harassManager.onStart();
	m_combatCommander.onStart();
}

void GameCommander::onFrame()
{
    m_timer.start();

	handleDTdetections();
    handleUnitAssignments();
    m_productionManager.onFrame();
    m_scoutManager.onFrame();
	m_harassManager.onFrame();
    m_combatCommander.onFrame(m_combatUnits);

    drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
    drawGameInformation(4, 1);
}

void GameCommander::drawGameInformation(int x, int y)
{
    std::stringstream ss;
    ss << "Players: " << "\n";
    ss << "Strategy: " << m_bot.Config().StrategyName << "\n";
    ss << "Map Name: " << "\n";
    ss << "Time: " << "\n";
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	m_validUnits.clear();
	m_combatUnits.clear();
	m_harassUnits.clear();
	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	setScoutUnits();
	setHarassUnits();
	setCombatUnits();
}

bool GameCommander::isAssigned(const sc2::Unit * unit) const
{
	return     (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
		|| (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end())
		|| (std::find(m_harassUnits.begin(), m_harassUnits.end(), unit) != m_harassUnits.end());
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	// make sure the unit is completed and alive and usable
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (m_bot.GetUnit(unit->tag) && unit->is_alive && unit->last_seen_game_loop == m_bot.Observation()->GetGameLoop())
		{
			m_validUnits.push_back(unit);
		}
	}
}

void GameCommander::setScoutUnits()
{
	// if we haven't set a scout unit, do it
	if (!m_initialScoutSet && m_scoutUnits.empty())
	{
		// if it exists
		if (shouldSendInitialScout())
		{
			//We only need the worker scout for pocket bases
			if (!m_bot.Map().hasPocketBase())
			{
				m_initialScoutSet = true;
				return;
			}
			// grab the closest worker to the supply provider to send to scout
			const sc2::Unit * workerScout = m_bot.Workers().getClosestMineralWorkerTo(m_bot.GetStartLocation());

			// if we find a worker (which we should) add it to the scout units
			if (workerScout)
			{
				m_scoutManager.setWorkerScout(workerScout);
				assignUnit(workerScout, m_scoutUnits);
				m_initialScoutSet = true;
				return;
			}
		}
	}

	if (m_scoutManager.getNumScouts() == -1)
	{
		m_productionManager.requestScout();
		m_scoutManager.scoutRequested();
	}
	else if (m_scoutManager.getNumScouts() == 0)
	{
		for (auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit) && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
			{
				m_scoutManager.setScout(unit);
				m_scoutUnits.clear();
				assignUnit(unit, m_scoutUnits);
				return;
			}
		}
	}
}

bool GameCommander::shouldSendInitialScout()
{
	switch (m_bot.GetPlayerRace(Players::Enemy))
	{
	case sc2::Race::Terran:  return false;
	case sc2::Race::Protoss: return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKS, true) == 1;
	case sc2::Race::Zerg:    return false;
	default: return false;
	}
}

void GameCommander::setHarassUnits()
{
	//Re-assign the ones already in the harass manager
	const sc2::Unit * medivac = m_harassManager.getMedivac();
	if (medivac)
	{
		assignUnit(medivac, m_harassUnits);
	}
	sc2::Units marines = m_harassManager.getMarines();
	if (marines.size()>0)
	{
		for (auto & m : marines)
		{
			assignUnit(m, m_harassUnits);
		}
	}
	const sc2::Unit * liberator = m_harassManager.getLiberator();
	if (liberator)
	{
		assignUnit(liberator, m_harassUnits);
	}
	
	//We only start harassing after we saw two bases. Otherwise it might be a 1 base allin
	//We should only can add units, if we are not under attack or if we have many units already
	if (m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).size()>1 || ( !m_combatCommander.underAttack() || m_validUnits.size() > manyUnits))
	{
		sc2::Units enemies = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Enemy);
		for (auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit))
			{
				if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC && m_harassManager.needMedivac())
				{
					if (m_harassManager.setMedivac(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
				else if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MARINE && unit->health == unit->health_max && m_harassManager.needMarine())
				{
					//If the marine is currently close to an anti air enemy, the medivac does not know what to do
					bool tooClose = false;
					for (auto & e : enemies)
					{
						if (Util::Dist(e->pos, unit->pos) < Util::GetUnitTypeSight(unit->unit_type, m_bot))
						{
							tooClose = true;
						}
					}
					if (!tooClose && m_harassManager.setMarine(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
				else if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_LIBERATOR && m_harassManager.needLiberator())
				{
					if (m_harassManager.setLiberator(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
			}
		}
	}
}


// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
    for (auto & unit : m_validUnits)
    {
        BOT_ASSERT(unit, "Have a null unit in our valid units\n");
        if (!isAssigned(unit) && Util::IsCombatUnitType(unit->unit_type, m_bot))
        {
            assignUnit(unit, m_combatUnits);
        }
    }
}

void GameCommander::onUnitCreate(const sc2::Unit * unit)
{
	if (Util::IsCombatUnitType(unit->unit_type,m_bot))
	{
		sc2::Point2D pos(m_bot.Bases().getRallyPoint());
		if (Util::Dist(unit->pos, pos) > 5)
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				Micro::SmartMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
				return;
			}
			else
			{
				std::map<const sc2::Unit *, UnitInfo> knownUnits = m_bot.UnitInfo().getUnitInfoMap(Players::Self);
				if (knownUnits.find(unit) != knownUnits.end())
				{
					//We have seen this one already
					return;
				}

				const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BUNKER }));
				for (auto & b : Bunker)
				{
					if (b->build_progress==1.0f && b->cargo_space_taken != b->cargo_space_max)
					{
						Micro::SmartRightClick(unit, b, m_bot);
						m_bot.Actions()->UnitCommand(b, sc2::ABILITY_ID::LOAD, unit);
						return;
					}
				}
				Micro::SmartAttackMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
				return;
			}
		}
	}
	else if (Util::IsTownHallType(unit->unit_type))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::OnBuildingConstructionComplete(const sc2::Unit * unit)
{
	if (Util::IsTownHallType(unit->unit_type))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::onUnitDestroy(const sc2::Unit * unit)
{
    //_productionManager.onUnitDestroy(unit);
}

void GameCommander::OnUnitEnterVision(const sc2::Unit * unit)
{
	if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_LIBERATOR || unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_BANSHEE || unit->unit_type.ToType() == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
	{
		m_productionManager.requestVikings();
	}
}

void GameCommander::OnDTdetected(const sc2::Point2D pos)
{
	m_productionManager.requestScan();
	m_DTdetections.push_back(timePlace(m_bot.Observation()->GetGameLoop(),pos));
}

void GameCommander::assignUnit(const sc2::Unit * unit, std::vector<const sc2::Unit *> & units)
{
    if (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end())
    {
        m_scoutUnits.erase(std::remove(m_scoutUnits.begin(), m_scoutUnits.end(), unit), m_scoutUnits.end());
    }
    else if (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
    {
        m_combatUnits.erase(std::remove(m_combatUnits.begin(), m_combatUnits.end(), unit), m_combatUnits.end());
    }
	else if (std::find(m_harassUnits.begin(), m_harassUnits.end(), unit) != m_harassUnits.end())
	{
		m_harassUnits.erase(std::remove(m_harassUnits.begin(), m_harassUnits.end(), unit), m_harassUnits.end());
	}

    units.push_back(unit);
}

const ProductionManager & GameCommander::Production() const
{
	return m_productionManager;
}

void GameCommander::handleDTdetections()
{
	if (m_bot.Observation()->GetGameLoop() == 4032)
	{
		m_productionManager.requestScan();
	}
	if (m_DTdetections.size() > 0)
	{
		//If the detection is too old or if we are already scanning discard it.
		const uint32_t currentTime = m_bot.Observation()->GetGameLoop();
		const std::vector<sc2::Effect> effects = m_bot.Observation()->GetEffects();
		std::vector<sc2::Point2D> scanPoints;
		for (auto & effect : effects)
		{
			if (sc2::EffectID(effect.effect_id).ToType() == sc2::EFFECT_ID::SCANNERSWEEP)
			{
				scanPoints.push_back(effect.positions.front());
			}
		}
		const float scanRadius = m_bot.Observation()->GetEffectData()[sc2::EffectID(sc2::EFFECT_ID::SCANNERSWEEP)].radius;
		int numRemovedDetections = 0;
		m_DTdetections.erase(std::remove_if(m_DTdetections.begin(), m_DTdetections.end(), [currentTime, scanPoints, scanRadius,&numRemovedDetections](timePlace tp) {
			if (currentTime - tp.m_time > 112) //5 sec
			{
				++numRemovedDetections;
				return true;
			}
			for (const auto & p : scanPoints)
			{
				if (Util::Dist(tp.m_place, p) < scanRadius)
				{
					++numRemovedDetections;
					return true;
				}
			}
			return false;
		}), m_DTdetections.end());
		m_productionManager.usedScan(numRemovedDetections);
		if (m_DTdetections.empty())
		{
			return;
		}
		const sc2::Units CommandCenters = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND));
		for (const auto & unit : CommandCenters)
		{
			if (unit->build_progress == 1.0f)
			{
				if (unit->energy >= 50)
				{
					int nearbyUnits = 0;
					for (const auto & unit : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self))
					{
						if (Util::IsCombatUnitType(unit->unit_type,m_bot) &&  Util::Dist(m_DTdetections.back().m_place, unit->pos) < scanRadius)
						{
							++nearbyUnits;
						}
					}
					if (nearbyUnits > 5)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::EFFECT_SCAN, m_DTdetections.back().m_place);
						m_productionManager.usedScan();
						return;
					}
				}
			}
		}
	}
}

