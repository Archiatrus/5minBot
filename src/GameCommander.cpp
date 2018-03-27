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

	handleDTdetections();
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
	m_harassUnits.clear();
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
	return     (std::find_if(m_combatUnits.begin(), m_combatUnits.end(), [unit](auto & newUnit) {return unit->getTag() == newUnit->getTag(); }) != m_combatUnits.end())
		|| (std::find_if(m_scoutUnits.begin(), m_scoutUnits.end(), [unit](auto & newUnit) {return unit->getTag() == newUnit->getTag(); }) != m_scoutUnits.end())
		|| (std::find_if(m_harassUnits.begin(), m_harassUnits.end(), [unit](auto & newUnit) {return unit->getTag() == newUnit->getTag(); }) != m_harassUnits.end())
		|| isShuttle(unit);
}

bool GameCommander::isShuttle(CUnit_ptr unit) const
{
	if (unit->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
	{
		return false;
	}
	for (const auto & s : m_shuttles)
	{
		if (s->isShuttle(unit))
		{
			return true;
		}
	}
	return false;
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
		if (m_bot.GetUnit(unit->getTag()) && unit->isAlive() && unit->getLastSeenGameLoop() == m_bot.Observation()->GetGameLoop() && unit->getUnitType().ToType()!=sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
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
				if (unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC && unit->getHealth() == unit->getHealthMax())
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
	if (!m_initialScoutSet && m_scoutUnits.empty())
	{
		// if it exists
		if (shouldSendInitialScout())
		{
			//We only need the worker scout for pocket bases
			//...or maybe not
			//if (!m_bot.Map().hasPocketBase())
			//{
			//	m_initialScoutSet = true;
			//	return;
			//}
			// grab the closest worker to the supply provider to send to scout
			CUnit_ptr workerScout = m_bot.Workers().getClosestMineralWorkerTo(m_bot.GetStartLocation());

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
		for (const auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit) && unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_REAPER)
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
	case sc2::Race::Zerg:	return false;
	default: return false;
	}
}

void GameCommander::setHarassUnits()
{
	//Re-assign the ones already in the harass manager
	CUnit_ptr medivac = m_harassManager.getMedivac();
	if (medivac)
	{
		assignUnit(medivac, m_harassUnits);
	}
	CUnits marines = m_harassManager.getMarines();
	if (marines.size()>0)
	{
		for (const auto & m : marines)
		{
			assignUnit(m, m_harassUnits);
		}
	}
	CUnit_ptr liberator = m_harassManager.getLiberator();
	if (liberator)
	{
		assignUnit(liberator, m_harassUnits);
	}
	CUnit_ptr widowMine = m_harassManager.getWidowMine();
	if (widowMine)
	{
		assignUnit(widowMine, m_harassUnits);
	}
	
	//We only start harassing after we saw two bases. Otherwise it might be a 1 base allin
	//We should only can add units, if we are not under attack or if we have many units already
	if (m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).size()>1 || ( !m_combatCommander.underAttack() || m_validUnits.size() > manyUnits))
	{
		CUnits enemies = m_bot.UnitInfo().getUnits(Players::Enemy);
		for (const auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit))
			{
				if (unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC && m_harassManager.needMedivac())
				{
					if (m_harassManager.setMedivac(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
				//We only assign marines, after we have a medivac
				else if (medivac && unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MARINE && unit->getHealth() == unit->getHealthMax() && m_harassManager.needMarine())
				{
					//If the marine is currently close to an anti air enemy, the medivac does not know what to do
					bool tooClose = false;
					for (auto & e : enemies)
					{
						if (Util::Dist(e->getPos(), unit->getPos()) < unit->getSightRange())
						{
							tooClose = true;
						}
					}
					if (!tooClose && m_harassManager.setMarine(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
				else if (unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_LIBERATOR && m_harassManager.needLiberator())
				{
					if (m_harassManager.setLiberator(unit))
					{
						assignUnit(unit, m_harassUnits);
					}
				}
				else if (unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_WIDOWMINE && m_harassManager.needWidowMine())
				{
					if (m_harassManager.setWidowMine(unit))
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
				Micro::SmartMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
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
				Micro::SmartAttackMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
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

void GameCommander::OnDTdetected(const sc2::Point2D pos)
{
	m_productionManager.requestScan();
	m_DTdetections.push_back(timePlace(m_bot.Observation()->GetGameLoop(),pos));
}

void GameCommander::assignUnit(CUnit_ptr unit, CUnits & units)
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
			scanPos=m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy)->getDepotPosition();
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
						if (combatUnit->isCombatUnit() &&  Util::Dist(m_DTdetections.back().m_place, combatUnit->getPos()) < scanRadius)
						{
							++nearbyUnits;
						}
					}
					if (nearbyUnits > 5)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::EFFECT_SCAN, m_DTdetections.back().m_place,m_bot);
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