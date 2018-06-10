#include "GameCommander.h"
#include "CCBot.h"
#include "Util.h"

//Only harass if we have more than
const int manyUnits = 25;

timePlace::timePlace(uint32_t time, sc2::Point2D place) : m_time(time), m_place(place)
{
}

GameCommander::GameCommander(CCBot & bot)
	: m_bot				 (bot)
	, m_productionManager   (bot)
	, m_scoutManager		(bot)
	, m_harassManager		(bot)
	, m_combatCommander	 (bot)
	, m_initialScoutSet	 (false)
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

	handleScans();
	handleShuttleService();
	handleUnitAssignments();
	m_productionManager.onFrame();
	m_scoutManager.onFrame();
	m_harassManager.onFrame();
	m_combatCommander.onFrame(m_combatUnits);

	drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
	drawGameInformation(sc2::Point2D(4.0f, 1.0f));
}

void GameCommander::drawGameInformation(const sc2::Point2D pos)
{
	std::stringstream ss;
	ss << "Players: " << "\n";
	ss << "Strategy: " << m_bot.Config().StrategyName << "\n";
	ss << "Map Name: " << "\n";
	ss << "Time: " << "\n";
	Drawing::drawTextScreen(m_bot, pos, ss.str());
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	m_validUnits.clear();
	m_combatUnits.clear();
	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	setScoutUnits();
	setHarassUnits();
	setShuttles();
	setCombatUnits();
}

bool GameCommander::isAssigned(CUnit_ptr unit) const
{
	return     (std::find_if(m_combatUnits.begin(), m_combatUnits.end(), [&](auto & newUnit) {return unit->getTag() == newUnit->getTag(); }) != m_combatUnits.end())
		|| isScout(unit)
		|| isHarassUnit(unit)
		|| isShuttle(unit);
}

bool GameCommander::isShuttle(CUnit_ptr unit) const
{
	return unit->getOccupation().first == CUnit::Occupation::Shuttle;
}

bool GameCommander::isScout(CUnit_ptr unit) const
{
	return unit->getOccupation().first == CUnit::Occupation::Scout || (unit->getOccupation().first == CUnit::Occupation::Worker && unit->getOccupation().second == WorkerJobs::Scout);
}

bool GameCommander::isHarassUnit(CUnit_ptr unit) const
{
	return unit->getOccupation().first == CUnit::Occupation::Harass;
}

void GameCommander::handleShuttleService()
{
	
	m_shuttles.erase(std::remove_if(m_shuttles.begin(), m_shuttles.end(),[](std::shared_ptr<shuttle> s){
		//Here is the shuttle handling hidden
		s->hereItGoes();
		return s->getShuttleStatus()==ShuttleStatus::Done;
	}), m_shuttles.end());
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	// make sure the unit is completed and alive and usable
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (m_bot.GetUnit(unit->getTag()) && unit->isAlive() && !unit->isWorker() && !unit->isBuilding() && unit->getLastSeenGameLoop() == m_bot.Observation()->GetGameLoop() && !unit->isType(sc2::UNIT_TYPEID::TERRAN_KD8CHARGE))
		{
			m_validUnits.push_back(unit);
		}
	}
}

void GameCommander::setShuttles()
{
	for (auto & s : m_shuttles)
	{
		if (s->needShuttleUnit())
		{
			for (const auto & unit : m_validUnits)
			{
				if (unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC) && unit->getHealth() == unit->getHealthMax())
				{
					if (!isAssigned(unit))
					{
						s->assignShuttleUnit(unit);
						break;
					}
				}
			}
		}
	}
}

void GameCommander::setScoutUnits()
{
	// if we haven't set a scout unit, do it
	if (!m_initialScoutSet)
	{
		// if it exists
		if (shouldSendInitialScout())
		{
			// grab the closest worker to the supply provider to send to scout
			CUnit_ptr workerScout = m_bot.Workers().getClosestMineralWorkerTo(m_bot.GetStartLocation());

			// if we find a worker (which we should) add it to the scout units
			if (workerScout)
			{
				m_scoutManager.setWorkerScout(workerScout);
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
		for (const auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit) && unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_REAPER)
			{
				m_scoutManager.setScout(unit);
				return;
			}
		}
	}
}

bool GameCommander::shouldSendInitialScout()
{
	switch (m_bot.GetPlayerRace(Players::Enemy))
	{
	case sc2::Race::Terran:  return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED, true) == 1;
	case sc2::Race::Protoss: return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED, true) == 1;
	case sc2::Race::Zerg:	return false;
	default: return false;
	}
}

void GameCommander::setHarassUnits()
{
	//We only start harassing after we saw two bases. Otherwise it might be a 1 base allin
	//We should only can add units, if we are not under attack or if we have many units already
	if (m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).size()>1 || ( !m_combatCommander.underAttack() || m_validUnits.size() > manyUnits))
	{
		CUnits enemies = m_bot.UnitInfo().getUnits(Players::Enemy);
		CUnits medivacs = m_harassManager.getMedivacs();
		for (const auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit))
			{
				if (unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC) && m_harassManager.needMedivac() && unit->getCargoSpaceTaken()==0)
				{
					int freeMarines = 0;
					for (const auto & marine : m_validUnits)
					{
						if (marine->isType(sc2::UNIT_TYPEID::TERRAN_MARINE) && marine->getOccupation().first == CUnit::Occupation::Combat)
						{
							++freeMarines;
						}
					}
					if (freeMarines >= 16)
					{
						m_harassManager.setMedivac(unit);
					}
				}
				//We only assign marines, after we have a medivac
				else if (medivacs.size()>0 && unit->isType(sc2::UNIT_TYPEID::TERRAN_MARINE) && unit->getHealth() == unit->getHealthMax() && m_harassManager.needMarine())
				{
					//If the marine is currently close to an anti air enemy, the medivac does not know what to do
					bool tooClose = false;
					for (const auto & e : enemies)
					{
						if (Util::Dist(e->getPos(), unit->getPos()) < unit->getSightRange())
						{
							tooClose = true;
							break;
						}
					}
					if (!tooClose)
					{
						m_harassManager.setMarine(unit);
					}
				}
				else if (unit->isType(sc2::UNIT_TYPEID::TERRAN_LIBERATOR))
				{
					m_harassManager.setLiberator(unit);
				}
				else if (unit->isType(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE) && m_harassManager.needWidowMine())
				{
					m_harassManager.setWidowMine(unit);
				}
			}
		}
		if (m_harassManager.needLiberator())
		{
			m_productionManager.requestLiberator();
		}
	}
}


// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
	for (const auto & unit : m_validUnits)
	{
		BOT_ASSERT(unit, "Have a null unit in our valid units\n");
		if (!isAssigned(unit) && Util::IsCombatUnitType(unit->getUnitType(), m_bot))
		{
			assignUnit(unit, m_combatUnits);
		}
	}
}

void GameCommander::onUnitCreate(CUnit_ptr unit)
{
	if (Util::IsCombatUnitType(unit->getUnitType(),m_bot))
	{
		sc2::Point2D pos(m_bot.Bases().getRallyPoint());
		if (Util::Dist(unit->getPos(), pos) > 5)
		{
			if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				Micro::SmartMove(unit, pos, m_bot);
				return;
			}
			else
			{
				CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
				for (auto & b : Bunker)
				{
					if (b->getBuildProgress()==1.0f && b->getCargoSpaceTaken() != b->getCargoSpaceMax())
					{
						Micro::SmartRightClick(unit, b, m_bot);
						Micro::SmartAbility(b, sc2::ABILITY_ID::LOAD, unit,m_bot);
						return;
					}
				}
				Micro::SmartAttackMove(unit, pos, m_bot);
				return;
			}
		}
	}
	else if (Util::IsTownHallType(unit->getUnitType()))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::OnBuildingConstructionComplete(CUnit_ptr unit)
{
	if (Util::IsTownHallType(unit->getUnitType()))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::onUnitDestroy(CUnit_ptr unit)
{
	//_productionManager.onUnitDestroy(unit);
}

void GameCommander::OnUnitEnterVision(CUnit_ptr unit)
{
	if (unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_LIBERATOR || unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_BANSHEE || unit->getUnitType().ToType() == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
	{
		m_productionManager.requestVikings();
	}
}

void GameCommander::OnDetectedNeeded(const sc2::Point2D pos)
{
	m_productionManager.requestScan();
	m_needDetections.push_back(timePlace(m_bot.Observation()->GetGameLoop(),pos));
}

void GameCommander::assignUnit(CUnit_ptr unit, CUnits & units)
{
	if (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
	{
		m_combatUnits.erase(std::remove(m_combatUnits.begin(), m_combatUnits.end(), unit), m_combatUnits.end());
	}

	units.push_back(unit);
}

const ProductionManager & GameCommander::Production() const
{
	return m_productionManager;
}

void GameCommander::handleScans()
{
	if (m_bot.Observation()->GetGameLoop() == 5508)
	{
		m_productionManager.requestScan();
	}
	if (m_bot.Observation()->GetGameLoop() == 6732)
	{
		const sc2::Point2D pos = m_bot.Bases().getNextExpansion(Players::Enemy);
		sc2::Point2D scanPos;
		if (m_bot.Observation()->GetVisibility(pos) == sc2::Visibility::Hidden && m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).size() < 2)
		{
			scanPos=pos;
		}
		else
		{
			scanPos=m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy)->getCenterOfBase();
			if (m_bot.Observation()->GetVisibility(scanPos) != sc2::Visibility::Hidden)
			{
				return;
			}
		}
		CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
		for (const auto & unit : CommandCenters)
		{
			if (unit->isCompleted())
			{
				if (unit->getEnergy() >= 50)
				{
					Micro::SmartAbility(unit, sc2::ABILITY_ID::EFFECT_SCAN, scanPos, m_bot);
					m_productionManager.usedScan();
					return;
				}
			}
		}
	}
	for (const auto & effect : m_bot.Observation()->GetEffects())
	{
		if (static_cast<sc2::EFFECT_ID>(effect.effect_id) == sc2::EFFECT_ID::LURKERATTACK)
		{
			OnDetectedNeeded(effect.positions.front());
		}
	}
	if (m_needDetections.size() > 0)
	{
		//If the detection is too old or if we are already scanning discard it.
		const uint32_t currentTime = m_bot.Observation()->GetGameLoop();
		std::vector<sc2::Point2D> scanPoints;
		for (auto & effect : m_bot.Observation()->GetEffects())
		{
			if (sc2::EffectID(effect.effect_id).ToType() == sc2::EFFECT_ID::SCANNERSWEEP)
			{
				scanPoints.push_back(effect.positions.front());
			}
		}
		const float scanRadius = m_bot.Observation()->GetEffectData()[sc2::EffectID(sc2::EFFECT_ID::SCANNERSWEEP)].radius;
		int numRemovedDetections = 0;
		m_needDetections.erase(std::remove_if(m_needDetections.begin(), m_needDetections.end(), [currentTime, scanPoints, scanRadius,&numRemovedDetections](timePlace tp) {
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
		}), m_needDetections.end());
		m_productionManager.usedScan(numRemovedDetections);
		if (m_needDetections.empty())
		{
			return;
		}
		CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self,sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
		for (const auto & unit : CommandCenters)
		{
			if (unit->getBuildProgress() == 1.0f)
			{
				if (unit->getEnergy() >= 50)
				{
					int nearbyUnits = 0;
					for (const auto & combatUnit : m_bot.UnitInfo().getUnits(Players::Self))
					{
						if (combatUnit->isCombatUnit() &&  Util::Dist(m_needDetections.back().m_place, combatUnit->getPos()) < scanRadius)
						{
							++nearbyUnits;
						}
					}
					if (nearbyUnits > 5)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::EFFECT_SCAN, m_needDetections.back().m_place,m_bot);
						m_productionManager.usedScan();
						return;
					}
				}
			}
		}
	}
}

void GameCommander::requestGuards(const bool req)
{
	m_combatCommander.requestGuards(req);
}

std::shared_ptr<shuttle> GameCommander::requestShuttleService(CUnits passengers, const sc2::Point2D targetPos)
{
	std::shared_ptr<shuttle> newShuttle = std::make_shared<shuttle>(&m_bot, passengers, targetPos);
	m_shuttles.push_back(newShuttle);
	return newShuttle;
}

const bool GameCommander::underAttack() const
{
	return m_combatCommander.underAttack();
}