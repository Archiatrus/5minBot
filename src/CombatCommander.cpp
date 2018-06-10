#include "CombatCommander.h"
#include "Util.h"
#include "CCBot.h"
#include "Micro.h"
#include "CUnit.h"

const size_t IdlePriority = 1;
const size_t GuardDutyPriority = 2;
const size_t AttackPriority = 3;
const size_t BaseDefensePriority = 4;
const size_t ScoutDefensePriority = 5;
const size_t DropPriority = 6;

CombatCommander::CombatCommander(CCBot & bot)
	: m_bot(bot)
	, m_squadData(bot)
	, m_initialized(false)
	, m_attackStarted(false)
	, m_needGuards(false)
	, m_underAttack(false)
{

}

void CombatCommander::onStart()
{
	m_squadData.clearSquadData();

	SquadOrder idleOrder(SquadOrderTypes::Idle, m_bot.GetStartLocation(), 5, "Chill Out");
	m_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority, m_bot));

	// the main attack squad that will pressure the enemy's closest base location
	SquadOrder mainAttackOrder(SquadOrderTypes::Attack, sc2::Point2D(0.0f, 0.0f), 25, "Attack Enemy Base");
	m_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority, m_bot));

	// the scout defense squad will handle chasing the enemy worker scout
	SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, m_bot.GetStartLocation(), 25, "Get the scout");
	m_squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority, m_bot));

	// the guard duty squad will handle securing the area for a new expansion
	SquadOrder guardDutyOrder(SquadOrderTypes::GuardDuty, sc2::Point2D(0.0f, 0.0f), 25, "Guard Duty");
	m_squadData.addSquad("GuardDuty", Squad("GuardDuty", enemyScoutDefense, ScoutDefensePriority, m_bot));
}

bool CombatCommander::isSquadUpdateFrame()
{
	return true;
}

void CombatCommander::onFrame(const CUnits & combatUnits)
{
	if (!m_attackStarted)
	{
		m_attackStarted = shouldWeStartAttacking();
	}

	m_combatUnits = combatUnits;

	if (isSquadUpdateFrame())
	{
		updateIdleSquad();
		//updateScoutDefenseSquad();
		updateDefenseSquads();
		updateAttackSquads();
		updateGuardSquads();
	}
	checkForProxyOrCheese();
	m_squadData.onFrame();
}

bool CombatCommander::shouldWeStartAttacking()
{
	// TODO: make this more clever
	// For now: start attacking when we have more than 10 combat units
	auto upgrades = m_bot.Observation()->GetUpgrades();
	for (auto & upgrade : upgrades)
	{
		if (upgrade.ToType() == sc2::UPGRADE_ID::STIMPACK && m_combatUnits.size() >= m_bot.Config().CombatUnitsForAttack && (m_bot.Observation()->GetFoodUsed()>198 || m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) >= m_bot.UnitInfo().getUnitTypeCount(Players::Enemy, sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)))
		{
			return true;
		}
	}
	return m_combatUnits.size() >= 100;
}

void CombatCommander::updateIdleSquad()
{
	Squad & idleSquad = m_squadData.getSquad("Idle");

	for (const auto & unit : m_combatUnits)
	{
		// if it hasn't been assigned to a squad yet, put it in the low priority idle squad
		if (m_squadData.canAssignUnitToSquad(unit, idleSquad))
		{
			idleSquad.addUnit(unit);
		}
	}
}

void CombatCommander::updateAttackSquads()
{
	if (!m_attackStarted)
	{
		return;
	}

	Squad & mainAttackSquad = m_squadData.getSquad("MainAttack");

	//We reassign every frame. Otherwise we can not change the jobs aka Harass, scout etc
	//mainAttackSquad.clear();

	for (const auto & unit : m_combatUnits)
	{   
		BOT_ASSERT(unit, "null unit in combat units");
		// get every unit of a lower priority and put it into the attack squad
		if (!unit->isWorker()
			&& !(unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_OVERLORD) 
			&& !(unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_QUEEN)
			&& m_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
		{
			m_squadData.assignUnitToSquad(unit, mainAttackSquad);
		}
	}

	SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 25, "Attack Enemy Base");
	mainAttackSquad.setSquadOrder(mainAttackOrder);
}

void CombatCommander::updateGuardSquads()
{
	Squad & guardSquad = m_squadData.getSquad("GuardDuty");

	if (!m_needGuards)
	{
		if (!guardSquad.isEmpty())
		{
			guardSquad.clear();
		}
		return;
	}

	if (m_attackStarted || m_bot.underAttack())
	{
		return;
	}

	for (const auto & unit : m_combatUnits)
	{
		BOT_ASSERT(unit, "null unit in combat units");
		// get every unit of a lower priority and put it into the attack squad
		if (!unit->isWorker()
			&& !(unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_OVERLORD)
			&& !(unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_QUEEN)
			&& m_squadData.canAssignUnitToSquad(unit, guardSquad))
		{
			m_squadData.assignUnitToSquad(unit, guardSquad);
		}
	}
	sc2::Point2D guardPosAux = m_bot.Bases().getNextExpansion(Players::Self);
	auto base = m_bot.Bases().getBaseLocation(guardPosAux);
	if (base)
	{
		const sc2::Point2D guardPos = base->getCenterOfBase() + Util::normalizeVector(base->getCenterOfBase() - base->getCenterOfRessources(), 5.0f);
		SquadOrder guardDutyOrder(SquadOrderTypes::GuardDuty, guardPos, 25, "Guard Duty");
		guardSquad.setSquadOrder(guardDutyOrder);
	}
}

void CombatCommander::updateScoutDefenseSquad()
{
	//if (m_combatUnits.empty())
	//{
	//    return;
	//}

	// if the current squad has units in it then we can ignore this
	Squad & scoutDefenseSquad = m_squadData.getSquad("ScoutDefense");

	// get the region that our base is located in
	const BaseLocation * myBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	BOT_ASSERT(myBaseLocation, "null self base location");

	// get all of the enemy units in this region
	CUnits enemyUnitsInRegion;
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (myBaseLocation->containsPosition(unit->getPos()))
		{
			enemyUnitsInRegion.push_back(unit);
		}
	}

	// if there's an enemy worker in our region then assign someone to chase him
	bool assignScoutDefender = (enemyUnitsInRegion.size() == 1) && enemyUnitsInRegion.front()->isWorker();

	// if our current squad is empty and we should assign a worker, do it
	if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
	{
		// the enemy worker that is attacking us
		CUnit_ptr enemyWorkerUnit = enemyUnitsInRegion.front();
		BOT_ASSERT(enemyWorkerUnit, "null enemy worker unit");

		// get our worker unit that is mining that is closest to it
		const CUnit_ptr workerDefender = findClosestWorkerTo(m_bot.Workers().getMineralWorkers(), enemyWorkerUnit->getPos());

		if (enemyWorkerUnit && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
			if (m_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
			{
				m_bot.Workers().setCombatWorker(workerDefender);
				m_squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
			}
		}
	}
	// if our squad is not empty and we shouldn't have a worker chasing then take him out of the squad
	else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
	{
		for (const auto & unit : scoutDefenseSquad.getUnits())
		{
			BOT_ASSERT(unit, "null unit in scoutDefenseSquad");

			Micro::SmartStop(unit, m_bot);
			if (unit->isWorker())
			{
				m_bot.Workers().finishedWithWorker(unit);
			}
		}

		scoutDefenseSquad.clear();
	}
}

void CombatCommander::updateDefenseSquads()
{
	m_underAttack = false;
	//if (m_combatUnits.empty())
	//{
	//    return;
	//}

	// for each of our occupied regions
	const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);
	for (const BaseLocation * myBaseLocation : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
	{
		// don't defend inside the enemy region, this will end badly when we are stealing gas or cannon rushing
		if (myBaseLocation == enemyBaseLocation)
		{
			continue;
		}

		sc2::Point2D basePosition = myBaseLocation->getCenterOfBase();

		// start off assuming all enemy units in region are just workers
		int numDefendersPerEnemyUnit = 2;

		// all of the enemy units in this region
		CUnits enemyUnitsInRegion;
		for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			// if it's an overlord, don't worry about it for defense, we don't care what they see
			if (unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_OVERLORD || !unit->isVisible())
			{
				continue;
			}

			if (Util::Dist(basePosition, unit->getPos()) < 25 + unit->getAttackRange())
			{
				enemyUnitsInRegion.push_back(unit);
			}
		}
		if (enemyUnitsInRegion.size() > 5)
		{
			m_underAttack = true;
		}

		// we can ignore the first enemy worker in our region since we assume it is a scout
		//......and miss the cannon rush
		/*
		for (const auto & unit : enemyUnitsInRegion)
		{
			BOT_ASSERT(unit, "null enemyt unit in region");

			if (false && unit->isWorker())
			{
				enemyUnitsInRegion.erase(std::remove(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), unit), enemyUnitsInRegion.end());
				break;
			}
		}
		*/

		// calculate how many units are flying / ground units
		int numEnemyFlyingInRegion = 0;
		int numEnemyGroundInRegion = 0;
		for (const auto & unit : enemyUnitsInRegion)
		{
			BOT_ASSERT(unit, "null enemyt unit in region");

			if (unit->isFlying())
			{
				numEnemyFlyingInRegion++;
			}
			else
			{
				if (unit->getUnitType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON)
				{
					numEnemyGroundInRegion++;
				}
				numEnemyGroundInRegion++;
			}
		}


		std::stringstream squadName;
		squadName << "Base Defense " << basePosition.x << " " << basePosition.y;

		// if there's nothing in this region to worry about
		if (enemyUnitsInRegion.empty())
		{
			// if a defense squad for this region exists, remove it
			if (m_squadData.squadExists(squadName.str()))
			{
				m_squadData.getSquad(squadName.str()).clear();
			}

			// and return, nothing to defend here
			continue;
		}
		else
		{
			// if we don't have a squad assigned to this region already, create one
			if (!m_squadData.squadExists(squadName.str()))
			{
				SquadOrder defendRegion(SquadOrderTypes::Defend, basePosition, 30.0f, "Defend Region!");
				m_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority, m_bot));
			}
		}

		// assign units to the squad
		if (m_squadData.squadExists(squadName.str()))
		{
			Squad & defenseSquad = m_squadData.getSquad(squadName.str());

			// figure out how many units we need on defense
			int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
			int groundDefensersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;

			updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded);
		}
		else
		{
			BOT_ASSERT(false, "Squad should have existed: %s", squadName.str().c_str());
		}
	}

	// for each of our defense squads, if there aren't any enemy units near the position, remove the squad
	std::set<std::string> uselessDefenseSquads;
	for (const auto & kv : m_squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		if (squad.isEmpty() || order.getType() != SquadOrderTypes::Defend)
		{
			continue;
		}

		bool enemyUnitInRange = false;
		for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (unit->getPos()!=sc2::Point3D() && Util::Dist(unit->getPos(), order.getPosition()) < order.getRadius())
			{
				enemyUnitInRange = true;
				break;
			}
		}
		bool onlyMedivacs = true;;
		for (const auto & unit : squad.getUnits())
		{
			if (!unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC))
			{
				onlyMedivacs = false;
				break;
			}
		}
		if (!enemyUnitInRange||onlyMedivacs)
		{
			m_squadData.getSquad(squad.getName()).clear();
		}
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	auto & squadUnits = defenseSquad.getUnits();

	// Remove repair worker
	CUnits repairWorkers;
	for (const auto & unit : squadUnits)
	{
		if (unit->isWorker() && m_bot.Workers().isRepairWorker(unit))
		{
			repairWorkers.push_back(unit);
		}
	}
	for (const auto & rw : repairWorkers)
	{
		defenseSquad.removeUnit(rw);
	}
	// TODO: right now this will assign arbitrary defenders, change this so that we make sure they can attack air/ground

	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
		defenseSquad.clear();
		return;
	}
	size_t defendersNeeded = flyingDefendersNeeded + groundDefendersNeeded;
	size_t defendersAdded = squadUnits.size();

	//If we are not attacking everybody can defend
	while (defendersNeeded > defendersAdded || !shouldWeStartAttacking())
	{
		const CUnit_ptr defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition());

		if (defenderToAdd)
		{
			m_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			defendersAdded+=2;
		}
		else
		{
			break;
		}
	}
	// Emergency draft of workers
	const CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
	const CUnits worker = m_bot.Workers().getMineralWorkers();
	size_t workerCounter = worker.size();
	while (defendersNeeded > defendersAdded && m_bot.UnitInfo().getNumCombatUnits(Players::Self)<20 && (Bunker.empty() || Bunker.front()->getCargoSpaceTaken()==0) && workerCounter>5)
	{
		const CUnit_ptr workerDefender = findClosestWorkerTo(worker,defenseSquad.getSquadOrder().getPosition());

		// grab it from the worker manager and put it in the squad
		if (workerDefender && m_squadData.canAssignUnitToSquad(workerDefender, defenseSquad))
		{
			m_bot.Workers().setCombatWorker(workerDefender);
			m_squadData.assignUnitToSquad(workerDefender, defenseSquad);
			++defendersAdded;
			--workerCounter;
		}
		else
		{
			break;
		}
	}
}

const CUnit_ptr CombatCommander::findClosestDefender(const Squad & defenseSquad, const sc2::Point2D & pos)
{
	CUnit_ptr closestDefender = nullptr;
	float minDistance = std::numeric_limits<float>::max();

	// TODO: add back special case of zergling rush defense

	for (const auto & unit : m_combatUnits)
	{
		BOT_ASSERT(unit, "null combat unit");

		if (!m_squadData.canAssignUnitToSquad(unit, defenseSquad))
		{
			continue;
		}

		float dist = Util::Dist(unit->getPos(), pos);
		if (!closestDefender || (dist < minDistance))
		{
			closestDefender = unit;
			minDistance = dist;
		}
	}

	return closestDefender;
}


void CombatCommander::drawSquadInformation()
{
	m_squadData.drawSquadInformation();
}

sc2::Point2D CombatCommander::getMainAttackLocation()
{
	const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);

	// First choice: Attack an enemy region if we can see units inside it
	if (enemyBaseLocation)
	{
		sc2::Point2D enemyBasePosition = enemyBaseLocation->getCenterOfBase();

		// If the enemy base hasn't been seen yet, go there.
		if (!m_bot.Map().isExplored(enemyBasePosition))
		{
			return enemyBasePosition;
		}
		else
		{
			// if it has been explored, go there if there are any visible enemy units there
			for (auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
			{
				if (enemyUnit->isAlive() && Util::Dist(enemyUnit->getPos(), enemyBasePosition) < 10) //Not to large. Maybe it is not in range
				{
					return enemyBasePosition;
				}
			}
		}
	}

	// Second choice: Attack clostest known enemy buildings
	const BaseLocation * base;
	if (enemyBaseLocation)
	{
		base = enemyBaseLocation;
	}
	else
	{
		base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	}
	int minDist = std::numeric_limits<int>::max();
	sc2::Point2D clostestEnemyBuildingPos(0.0f,0.0f);
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (unit->isBuilding() && unit->isAlive())
		{
			int dist = base->getGroundDistance(unit->getPos());
			if (minDist > dist)
			{
				minDist = dist;
				clostestEnemyBuildingPos = unit->getPos();
			}
		}
	}
	if (clostestEnemyBuildingPos.x != 0.0f)
	{
		return clostestEnemyBuildingPos;
	}

	// Third choice: Attack enemy units
	minDist = std::numeric_limits<int>::max();
	CUnit_ptr closestEnemy = nullptr;
	for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (enemyUnit->getPos() != sc2::Point3D())
		{
			int dist = base->getGroundDistance(enemyUnit->getPos());
			if (!closestEnemy || minDist > dist)
			{
				closestEnemy = enemyUnit;
				minDist = dist;
			}
		}
	}
	if (closestEnemy)
	{
		return closestEnemy->getPos();
	}
	// Fourth choice: We can't see anything so explore the map attacking along the way
	return m_bot.Map().getLeastRecentlySeenPosition();
}

const CUnit_ptr CombatCommander::findClosestWorkerTo(const CUnits & unitsToAssign, const sc2::Point2D & target)
{
	CUnit_ptr closestMineralWorker = nullptr;
	float closestDist = std::numeric_limits<float>::max();

	// for each of our workers
	for (const auto & unit : unitsToAssign)
	{
		if (!unit)
		{
			continue;
		}
		BOT_ASSERT(unit, "unit to assign was null");

		if (!unit->isWorker())
		{
			continue;
		}

		// if it is a move worker
		if (m_bot.Workers().isFree(unit))
		{
			float dist = Util::Dist(unit->getPos(), target);

			if (dist < closestDist)
			{
				closestMineralWorker = unit;
				dist = closestDist;
			}
		}
	}

	return closestMineralWorker;
}

const bool CombatCommander::underAttack() const
{
	return m_underAttack;
}

void CombatCommander::requestGuards(const bool req)
{
	m_needGuards = req;
}

void CombatCommander::checkForProxyOrCheese()
{
	//const size_t numReactor = m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,false);
	//if (numReactor == 1)
	//{
	if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Terran || m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
	{
		const uint32_t time = m_bot.Observation()->GetGameLoop();
		if (time >= 2240)
		{
			const size_t bases = m_bot.UnitInfo().getUnitTypeCount(Players::Self, Util::getTownHallTypes());
			if (bases <= 1)
			{
				const CUnits enemyProduction = m_bot.UnitInfo().getUnits(Players::Enemy, std::vector<sc2::UnitTypeID>({ sc2::UNIT_TYPEID::TERRAN_BARRACKS,sc2::UNIT_TYPEID::PROTOSS_GATEWAY }));
				if (enemyProduction.empty())
				{
					m_underAttack = true;
					return;
				}
				if (enemyProduction.size() > 3)
				{
					const size_t numBunker = m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
					if (numBunker == 0)
					{
						m_underAttack = true;
						return;
					}
				}
				const sc2::Point2D home = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase();
				const std::vector<const BaseLocation *> enemyBases = m_bot.Bases().getStartingBaseLocations();
				for (const auto & enemy : enemyProduction)
				{
					for (const auto & enemyBase : enemyBases)
					{
						if (Util::DistSq(enemy->getPos(), home) < Util::DistSq(enemy->getPos(), enemyBase->getCenterOfBase()))
						{
							m_underAttack = true;
							return;
						}
					}
				}
			}
		}
	}
}