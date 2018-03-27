#include "ScoutManager.h"
#include "CCBot.h"
#include "Util.h"
#include "Micro.h"
#include "Drawing.h"
#include <queue>

const float reaperVisionRadius = 9;

bool firstCheckOurBases = true;
bool gotAttackedInEnemyRegion = false;

ScoutManager::ScoutManager(CCBot & bot)
	: m_bot			 (bot)
	, m_scoutUnit	   (nullptr)
	, m_numScouts	   (-1)
	, m_scoutUnderAttack(false)
	, m_scoutStatus	 ("None")
	, m_previousScoutHP (0.0f)
	, m_targetBasesPositions(std::queue<sc2::Point2D>())
	, m_foundProxy(false)
{

}

void ScoutManager::onStart()
{

}

void ScoutManager::onFrame()
{
	if (firstCheckOurBases)
	{
		checkOurBases();
	}
	else
	{
		if (m_scoutUnit && m_scoutUnit->isWorker())
		{
			m_bot.Workers().finishedWithWorker(m_scoutUnit);
			m_scoutUnit = nullptr;
			return;
		}
		moveScouts();
	}
	drawScoutInformation();
}

void ScoutManager::checkOurBases()
{
	CUnit_ptr & scout = m_scoutUnit;

	//No scout or scout dead
	if (!scout || !scout->isAlive())
	{
		if (m_numScouts == 1)
		{
			//We HAD a scout....
			m_scoutStatus = "Need new scout!";
			m_numScouts = -1;
			firstCheckOurBases = true;
			m_targetBasesPositions = std::queue<sc2::Point2D>();
		}
		if (scout && m_bot.Observation()->GetGameLoop() - scout->getLastSeenGameLoop() > 1224)
		{
			const CUnits rax = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKS);
			for (const auto & unit : rax)
			{
				for (const auto & order : unit->getOrders())
				{
					if (order.ability_id == sc2::ABILITY_ID::TRAIN_REAPER)
					{
						return;
					}
				}
			}
			m_numScouts = -1;
		}

		return;
	}
	//Do not annoy the reaper when he tries to jump
	if (scout->getOrders().size()>0 && scout->getOrders().front().ability_id == sc2::ABILITY_ID::MOVE && m_bot.Map().terrainHeight(scout->getPos().x, scout->getPos().y) != m_bot.Map().terrainHeight(scout->getPos().x + 2 * std::cos(scout->getFacing()), scout->getPos().y + 2 * std::sin(scout->getFacing())))
	{
		return;
	}

	if (m_targetBasesPositions.empty())
	{
		updateNearestUnoccupiedBases(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition(), Players::Self);
	}
	//Sometimes all bases are taken
	if (!m_targetBasesPositions.empty())
	{
		if (Util::DistSq(scout->getPos(), m_targetBasesPositions.front()) < 12.0f)
		{
			m_targetBasesPositions.pop();
		}
		else
		{
			//Whos there in sight?
			CUnits enemyUnitsInSight = scout->getEnemyUnitsInSight();

			//Do the actual scouting
			raiseAlarm(enemyUnitsInSight);

			//we do not want to flee if we find something in our territory
			//if (enemyTooClose(enemyUnitsInSight))
			//{

			//}
			//if there are combat units that can not attack us, but we can attack them, attack the weakest one.
			//else 
			if (!m_scoutUnit->isWorker())
			{
				if (attackEnemyCombat(enemyUnitsInSight))
				{

				}
				//if there are workers attack the weakest one
				else if (attackEnemyWorker(enemyUnitsInSight))
				{

				}
				// otherwise keep moving to the enemy base location
				else
				{
					Micro::SmartMove(m_scoutUnit, m_targetBasesPositions.front(), m_bot);
				}
			}
			// otherwise keep moving to the enemy base location
			else
			{
				Micro::SmartMove(m_scoutUnit, m_targetBasesPositions.front(), m_bot);
			}
		}
	}
	if (m_targetBasesPositions.empty())
	{
		firstCheckOurBases = false;
	}

}

void ScoutManager::setWorkerScout(CUnit_ptr tag)
{
	// if we have a previous worker scout, release it back to the worker manager
	if (m_scoutUnit && m_scoutUnit->isWorker())
	{
		m_bot.Workers().finishedWithWorker(m_scoutUnit);
	}

	m_scoutUnit = tag;
	m_bot.Workers().setScoutWorker(m_scoutUnit);
}

void ScoutManager::setScout(CUnit_ptr tag)
{
	if (m_scoutUnit && m_scoutUnit->isWorker())
	{
		m_bot.Workers().finishedWithWorker(m_scoutUnit);
	}
	m_numScouts=1;
	m_scoutUnit = tag;
}

void ScoutManager::drawScoutInformation()
{
	if (!m_bot.Config().DrawScoutInfo)
	{
		return;
	}

	std::stringstream ss;
	if (m_scoutUnit)
	{
		ss << "Scout Info: " << m_scoutStatus << " Health: " << m_scoutUnit->getHealth();
	}
	else
	{
		ss << "Scout Info: " << m_scoutStatus;
	}

	Drawing::drawTextScreen(m_bot,sc2::Point2D(0.1f, 0.6f), ss.str());
}

void ScoutManager::moveScouts()
{
	//for now we assume it is not a worker
	auto scout = m_scoutUnit;

	//No scout or scout dead
	if (!scout|| !scout->isAlive())
	{
		if (m_numScouts==1)
		{
			//We HAD a scout....
			if (scout && Util::Dist(scout->getPos(),m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getBasePosition()) > 50)
			{
				gotAttackedInEnemyRegion = true;
			}
			m_scoutStatus = "Need new scout!";
			m_numScouts = -1;
			firstCheckOurBases = true;
			m_targetBasesPositions = std::queue<sc2::Point2D>();
		}
		
		return;
	}
	//Do not annoy the reaper when he tries to jump
	if (scout->getOrders().size()>0&&scout->getOrders()[0].ability_id == sc2::ABILITY_ID::MOVE && m_bot.Map().terrainHeight(scout->getPos().x, scout->getPos().y) != m_bot.Map().terrainHeight(scout->getPos().x+2*std::cos(scout->getFacing()), scout->getPos().y+2*std::sin(scout->getFacing())))
	{
		return;
	}

	float scoutHP = scout->getHealth();
	
	if (scoutHP < scout->getHealthMax())
	{
		scoutDamaged();
		return;
	}
	
	// get the enemy base location, if we have one
	const BaseLocation * enemyBaseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Enemy);

	// if we know where the enemy region is and where our scout is
	if (enemyBaseLocation)
	{
		if (gotAttackedInEnemyRegion && m_targetBasesPositions.empty())
		{
			updateNearestUnoccupiedBases(enemyBaseLocation->getPosition(), Players::Enemy);
		}
		else if (m_targetBasesPositions.empty())
		{
			m_targetBasesPositions.push(enemyBaseLocation->getPosition());
		}


		//Whos there in sight?
		CUnits enemyUnitsInSight = m_scoutUnit->getEnemyUnitsInSight();
		// without words
		if (dontBlowYourselfUp())
		{

		}
		//if there is a unit and we are getting too close, throw granade and run
		else if (enemyTooClose(enemyUnitsInSight))
		{
			if (m_targetBasesPositions.size() > 0 && Util::Dist(scout->getPos(), m_targetBasesPositions.front()) < 20)
			{
				gotAttackedInEnemyRegion = true;
				m_targetBasesPositions.pop();
			}

		}
		//if there are combat units that can not attack us, but we can attack them, attack the weakest one.
		else if (attackEnemyCombat(enemyUnitsInSight))
		{

		}
		//if there are workers attack the weakest one
		else if (attackEnemyWorker(enemyUnitsInSight))
		{

		}
		// otherwise keep moving to the enemy base location
		else
		{
			if (m_targetBasesPositions.empty())
			{
				m_scoutStatus = "I am confused. No empty base left?!";
				return;
			}
			// move to the enemy region
			int scoutDistanceToEnemy = m_bot.Map().getGroundDistance(scout->getPos(), m_targetBasesPositions.front());
			if (scoutDistanceToEnemy <= 4 && scoutDistanceToEnemy >= 0)
			{
				m_scoutStatus = "Nothing here yet";
				m_targetBasesPositions.pop();
			}
			else
			{
				m_scoutStatus = "Enemy region known, going there";
				Micro::SmartMove(m_scoutUnit, m_targetBasesPositions.front(), m_bot);
			}
		}

	}
	// for each start location in the level
	else
	{
		m_scoutStatus = "Enemy base unknown, exploring";

		for (const BaseLocation * startLocation : m_bot.Bases().getStartingBaseLocations())
		{
			// if we haven't explored it yet then scout it out
			// TODO: this is where we could change the order of the base scouting, since right now it's iterator order
			if (!m_bot.Map().isExplored(startLocation->getPosition()))
			{

				Micro::SmartMove(m_scoutUnit, startLocation->getPosition(), m_bot);
				return;
			}
		}
	}
}

CUnit_ptr ScoutManager::closestEnemyWorkerTo() const
{
	if (!m_scoutUnit) { return nullptr; }

	CUnit_ptr enemyWorker = nullptr;
	float minDist = std::numeric_limits<float>::max();

	// for each enemy worker
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy, Util::getWorkerTypes()))
	{
		float dist = Util::Dist(unit->getPos(), m_scoutUnit->getPos());

		if (dist < minDist)
		{
			minDist = dist;
			enemyWorker = unit;
		}
	}

	return enemyWorker;
}

CUnit_ptr ScoutManager::closestEnemyCombatTo() const
{
	if (!m_scoutUnit) { return nullptr; }

	CUnit_ptr enemyUnit = nullptr;
	float minDist = std::numeric_limits<float>::max();

	// for each enemy worker
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (unit->isCombatUnit() && !unit->isFlying())
		{
			float dist = Util::Dist(unit->getPos(), m_scoutUnit->getPos());

			if (dist < minDist)
			{
				minDist = dist;
				enemyUnit = unit;
			}
		}
	}

	return enemyUnit;
}

bool ScoutManager::enemyTooClose(CUnits enemyUnitsInSight)
{
	bool tooClose = false;
	CUnits enemyPositions;
	//First gather all units that can shoot at the scout
	for (const auto &m_unit : enemyUnitsInSight)
	{
		const float dist = Util::Dist(m_unit->getPos(), m_scoutUnit->getPos());
		if (dist < m_unit->getAttackRange(m_scoutUnit) + 2.0f) //+2 to be on the save side
		{
			enemyPositions.push_back(m_unit);
			tooClose = true;
		}
	}
	//If there were any calculate the cluster center and flee in the other direction.
	if (tooClose)
	{
		m_scoutStatus = "Too close to the fire! Retreating";
		sc2::AvailableAbilities abilities = m_scoutUnit->getAbilities();
		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				Micro::SmartAbility(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE,enemyPositions.front(),m_bot);
				return tooClose;
			}
		}
		sc2::Point2D clusterPosition = Util::CalcCenter(enemyPositions);
		sc2::Point2D runningVector = Util::normalizeVector(m_scoutUnit->getPos() - clusterPosition, 5.0f);
		Micro::SmartMove(m_scoutUnit, m_scoutUnit->getPos() + runningVector, m_bot);
	}
	return tooClose;
}

bool ScoutManager::attackEnemyCombat(CUnits enemyUnitsInSight)
{
	bool attackingEnemy = false;
	CUnit_ptr lowestHealthUnit;
	for (const auto & unit : enemyUnitsInSight)
	{
		const float dist = Util::Dist(unit->getPos(), m_scoutUnit->getPos());
		if (unit->isCombatUnit() && dist < m_scoutUnit->getAttackRange(unit) + 1.0f) //+1 to be on the save side
		{
			if (attackingEnemy)
			{
				if (unit->getHealth() < lowestHealthUnit->getHealth())
				{
					lowestHealthUnit = unit;
				}
			}
			else
			{
				attackingEnemy = true;
				lowestHealthUnit = unit;
			}
		}
	}
	if (attackingEnemy)
	{
		m_scoutStatus = "Found a victim (combat). Attacking!";
		sc2::AvailableAbilities abilities = m_scoutUnit->getAbilities();
		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				Micro::SmartAbility(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, lowestHealthUnit,m_bot);
				return attackingEnemy;
			}
		}
		
		Micro::SmartKiteTarget(m_scoutUnit, lowestHealthUnit, m_bot);
		
	}
	return attackingEnemy;
}

bool ScoutManager::attackEnemyWorker(CUnits enemyUnitsInSight)
{
	CUnit_ptr lowestHealthUnitInRange=nullptr;
	CUnit_ptr lowestHealthUnitOutsideRange = nullptr;
	for (const auto & unit : enemyUnitsInSight)
	{
		if (unit->isWorker())
		{
			const float range = m_scoutUnit->getAttackRange(unit);
			const float dist = Util::Dist(unit->getPos(), m_scoutUnit->getPos());
			if (dist < range)
			{
				if (lowestHealthUnitInRange)
				{
					if (unit->getHealth() < lowestHealthUnitInRange->getHealth())
					{
						lowestHealthUnitInRange = unit;
					}
				}
				else
				{
					lowestHealthUnitInRange = unit;
				}
			}
			else
			{
				if (lowestHealthUnitOutsideRange)
				{
					if (unit->getHealth() < lowestHealthUnitOutsideRange->getHealth())
					{
						lowestHealthUnitOutsideRange = unit;
					}
				}
				else
				{
					lowestHealthUnitOutsideRange = unit;
				}
			}
		}
	}
	CUnit_ptr lowestHealthUnit = nullptr;
	lowestHealthUnitInRange ? lowestHealthUnit = lowestHealthUnitInRange : lowestHealthUnit = lowestHealthUnitOutsideRange;
	if (lowestHealthUnit)
	{
		m_scoutStatus = "Found a victim (worker). Attacking!";
		sc2::AvailableAbilities abilities = m_scoutUnit->getAbilities();
		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				Micro::SmartAbility(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, lowestHealthUnit,m_bot);
				return true;
			}
		}
		if (enemyUnitsInSight.size() > 1)
		{
			Micro::SmartKiteTarget(m_scoutUnit, lowestHealthUnit, m_bot);
		}
		//If it is only one worker do not flee
		else
		{
			Micro::SmartAttackUnit(m_scoutUnit, lowestHealthUnit, m_bot);
		}
		return true;
	}
	return false;
}

bool ScoutManager::enemyWorkerInRadiusOf(const sc2::Point2D & pos) const
{
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy,Util::getWorkerTypes()))
	{
		if (Util::Dist(unit->getPos(), pos) <= m_scoutUnit->getSightRange())
		{
			return true;
		}
	}

	return false;
}

void ScoutManager::scoutDamaged()
{
	auto scout = m_scoutUnit;
	if (scout->isWorker())
	{
		m_scoutStatus = "Too damaged. Retreating to base.";
		m_bot.Workers().finishedWithWorker(scout);
		m_numScouts = -1;
	}
	else
	{

		//Whos there in sight?
		CUnits enemyUnitsInSight = m_scoutUnit->getEnemyUnitsInSight();
		float sightDistance = m_scoutUnit->getSightRange();
		if (enemyUnitsInSight.size()>0)
		{
			m_scoutStatus = "Too damaged. Fleeing...";
			sc2::Point2D RunningVector = Util::normalizeVector(scout->getPos() - Util::CalcCenter(enemyUnitsInSight), sightDistance + 1.0f);
			Micro::SmartMove(scout, scout->getPos() + RunningVector,m_bot);
		}
		else
		{

		}
	}
}

sc2::Point2D ScoutManager::getFleePosition() const
{
	// TODO: make this follow the perimeter of the enemy base again, but for now just use home base as flee direction


	return m_bot.GetStartLocation();
}

int ScoutManager::getNumScouts()
{
	return m_numScouts;
}

void ScoutManager::updateNearestUnoccupiedBases(sc2::Point2D pos,int player)
{
	std::vector<const BaseLocation *> bases = m_bot.Bases().getBaseLocations();
	//We use that map is ordered
	std::map<int,const BaseLocation *> allTargetBases;
	int numBasesEnemy = 0;
	for (const auto & base : bases)
	{
		if (base->isOccupiedByPlayer(player))
		{
			numBasesEnemy++;
		}
		if (!(base->isOccupiedByPlayer(Players::Enemy)) && !(base->isOccupiedByPlayer(Players::Self)))
		{
			allTargetBases[base->getGroundDistance(pos)] = base;
		}
	}
	if (m_scoutUnit->isWorker())
	{
		auto baseIt = allTargetBases.begin();
		if (m_bot.Map().hasPocketBase())
		{
			baseIt++;
		}
		m_targetBasesPositions.push(baseIt->second->getBasePosition());
	}
	else
	{
		for (const auto & base : allTargetBases)
		{

			if (m_targetBasesPositions.size() < numBasesEnemy || numBasesEnemy == 0)
			{
				m_targetBasesPositions.push(base.second->getBasePosition());
			}
		}
	}
}

const bool ScoutManager::dontBlowYourselfUp() const
{
	CUnits grenades = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_KD8CHARGE);
	if (grenades.size() > 0)
	{
		for (const auto & g : grenades)
		{
			if (Util::Dist(g->getPos(), m_scoutUnit->getPos()) < 4)
			{
				//escape 3/4 pi
				const float n = 1.0f / std::sqrt(2.0f);
				sc2::Point2D vector(g->getPos().x - m_scoutUnit->getPos().x, g->getPos().y - m_scoutUnit->getPos().y);
				vector.x = -n*(vector.x + vector.y);
				vector.y = n*(vector.x - vector.y);
				const sc2::Point2D targetPos = m_scoutUnit->getPos() + vector;
				Micro::SmartMove(m_scoutUnit, targetPos, m_bot);
				return true;
			}
		}
	}
	return false;
}

void ScoutManager::scoutRequested()
{
	m_numScouts = 0;
}

void ScoutManager::raiseAlarm(CUnits enemyUnitsInSight)
{
	for (const auto & enemy : enemyUnitsInSight)
	{
		if (enemy->isBuilding())
		{
			m_foundProxy=true;
		}
	}
}
