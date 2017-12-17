#include "ScoutManager.h"
#include "CCBot.h"
#include "Util.h"
#include "Micro.h"
#include <queue>

const float reaperVisionRadius = 9;

bool firstCheckOurBases = true;
bool gotAttackedInEnemyRegion = false;

ScoutManager::ScoutManager(CCBot & bot)
    : m_bot             (bot)
    , m_scoutUnit       (nullptr)
    , m_numScouts       (-1)
    , m_scoutUnderAttack(false)
    , m_scoutStatus     ("None")
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
		if (m_scoutUnit && Util::IsWorkerType(m_scoutUnit->unit_type))
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
	auto scout = m_scoutUnit;

	//No scout or scout dead
	if (!scout || !scout->is_alive)
	{
		if (m_numScouts == 1)
		{
			//We HAD a scout....
			m_scoutStatus = "Need new scout!";
			m_numScouts = -1;
			firstCheckOurBases = true;
			m_targetBasesPositions = std::queue<sc2::Point2D>();
		}

		return;
	}
	//Do not annoy the reaper when he tries to jump
	if (scout->orders.size()>0 && scout->orders[0].ability_id == sc2::ABILITY_ID::MOVE && m_bot.Map().terrainHeight(scout->pos.x, scout->pos.y) != m_bot.Map().terrainHeight(scout->pos.x + 2 * std::cos(scout->facing), scout->pos.y + 2 * std::sin(scout->facing)))
	{
		return;
	}

	if (m_targetBasesPositions.empty())
	{
		updateNearestUnoccupiedBases(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition(), Players::Self);
	}
	if (Util::DistSq(scout->pos, m_targetBasesPositions.front()) < 12.0f)
	{
		m_targetBasesPositions.pop();
	}
	else
	{
		//Whos there in sight?
		std::vector<const sc2::Unit *> enemyUnitsInSight = getEnemyUnitsInSight(scout->pos);

		//Do the actual scouting
		raiseAlarm(enemyUnitsInSight);

		//we do not want to flee if we find something in our territory
		//if (enemyTooClose(enemyUnitsInSight))
		//{

		//}
		//if there are combat units that can not attack us, but we can attack them, attack the weakest one.
		//else 
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
	if (m_targetBasesPositions.empty())
	{
		firstCheckOurBases = false;
	}

}

void ScoutManager::setWorkerScout(const sc2::Unit * tag)
{
    // if we have a previous worker scout, release it back to the worker manager
    if (m_scoutUnit && Util::IsWorkerType(m_scoutUnit->unit_type))
    {
        m_bot.Workers().finishedWithWorker(m_scoutUnit);
    }

    m_scoutUnit = tag;
    m_bot.Workers().setScoutWorker(m_scoutUnit);
}

void ScoutManager::setScout(const sc2::Unit * tag)
{
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
		ss << "Scout Info: " << m_scoutStatus << " Health: " << m_scoutUnit->health;
	}
	else
	{
		ss << "Scout Info: " << m_scoutStatus;
	}

    m_bot.Map().drawTextScreen(sc2::Point2D(0.1f, 0.6f), ss.str());
}

void ScoutManager::moveScouts()
{
	//for now we assume it is not a worker
    auto scout = m_scoutUnit;

	//No scout or scout dead
    if (!scout|| !scout->is_alive)
	{
		if (m_numScouts==1)
		{
			//We HAD a scout....
			if (scout && Util::Dist(scout->pos,m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getBasePosition()) > 50)
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
	if (scout->orders.size()>0&&scout->orders[0].ability_id == sc2::ABILITY_ID::MOVE && m_bot.Map().terrainHeight(scout->pos.x, scout->pos.y) != m_bot.Map().terrainHeight(scout->pos.x+2*std::cos(scout->facing), scout->pos.y+2*std::sin(scout->facing)))
	{
		return;
	}

    float scoutHP = scout->health + scout->shield;
	
	if (scoutHP < scout->health_max)
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
		
		
        bool scoutInRangeOfenemy = enemyBaseLocation->containsPosition(scout->pos);

        // if the scout is in the enemy region
        if (true)
        {
			//Whos there in sight?
			std::vector<const sc2::Unit *> enemyUnitsInSight = getEnemyUnitsInSight(scout->pos);
			// without words
			if (dontBlowYourselfUp())
			{

			}
			//if there is a unit and we are getting too close, throw granade and run
			else if (enemyTooClose(enemyUnitsInSight))
			{
				if (Util::Dist(scout->pos,m_targetBasesPositions.front())<20)
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
				int scoutDistanceToEnemy = m_bot.Map().getGroundDistance(scout->pos, m_targetBasesPositions.front());
				if (scoutDistanceToEnemy <= 3 && scoutDistanceToEnemy>=0)
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
        // if the scout is not in the enemy region
        else
        {
			if (m_targetBasesPositions.empty())
			{
				m_scoutStatus = "I am confused. No empty base left?!";
				return;
			}
            // move to the enemy region
			int scoutDistanceToEnemy = m_bot.Map().getGroundDistance(scout->pos, m_targetBasesPositions.front());
			if (scoutDistanceToEnemy <= 3 && scoutDistanceToEnemy >= 0)
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

const sc2::Unit * ScoutManager::closestEnemyWorkerTo(const sc2::Point2D & pos) const
{
    if (!m_scoutUnit) { return nullptr; }

    UnitTag enemyWorkerTag = 0;
    float minDist = std::numeric_limits<float>::max();

    // for each enemy worker
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
    {
        if (Util::IsWorker(unit))
        {
            float dist = Util::Dist(unit->pos, m_scoutUnit->pos);

            if (dist < minDist)
            {
                minDist = dist;
                enemyWorkerTag = unit->tag;
            }
        }
    }

    return m_bot.GetUnit(enemyWorkerTag);
}

const sc2::Unit * ScoutManager::closestEnemyCombatTo(const sc2::Point2D & pos) const
{
	if (!m_scoutUnit) { return nullptr; }

	UnitTag enemyUnitTag = 0;
	float minDist = std::numeric_limits<float>::max();

	// for each enemy worker
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (Util::IsCombatUnit(unit, m_bot) && !unit->is_flying)
		{
			float dist = Util::Dist(unit->pos, m_scoutUnit->pos);

			if (dist < minDist)
			{
				minDist = dist;
				enemyUnitTag = unit->tag;
			}
		}
	}

	return m_bot.GetUnit(enemyUnitTag);
}

std::vector<const sc2::Unit *> ScoutManager::getEnemyUnitsInSight(const sc2::Point2D & pos) const
{
	std::vector<const sc2::Unit *> enemyUnitsInSight;
	if (!m_scoutUnit) { return enemyUnitsInSight; }

	UnitTag enemyUnitTag = 0;
	float sightDistance = Util::GetUnitTypeSight(m_scoutUnit->unit_type.ToType(),m_bot);
	// for each enemy unit (and building?)
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
			float dist = Util::Dist(unit->pos, m_scoutUnit->pos);

			if ((Util::IsCombatUnitType(unit->unit_type,m_bot) || Util::IsWorkerType(unit->unit_type)) && dist < sightDistance)
			{
				enemyUnitsInSight.push_back(unit);
			}
	}

	return enemyUnitsInSight;
}

bool ScoutManager::enemyTooClose(std::vector<const sc2::Unit *> enemyUnitsInSight)
{
	bool tooClose = false;
	std::vector<const sc2::Unit *> enemyPositions;
	//First gather all units that can shoot at the scout
	for (auto & unit : enemyUnitsInSight)
	{
		float dist = Util::Dist(unit->pos, m_scoutUnit->pos);
		if (dist < Util::GetUnitTypeRange(unit->unit_type, m_bot) + 1) //+1 to be on the save side
		{
			enemyPositions.push_back(unit);
			tooClose = true;
		}
	}
	//If there were any calculate the cluster center and flee in the other direction.
	if (tooClose)
	{
		m_scoutStatus = "Too close to the fire! Retreating";
		sc2::AvailableAbilities abilities = m_bot.Query()->GetAbilitiesForUnit(m_scoutUnit);
		for (auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				m_bot.Actions()->UnitCommand(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE,enemyPositions[0]);
				return tooClose;
			}
		}
		sc2::Point2D clusterPosition = Util::CalcCenter(enemyPositions);
		Micro::SmartMove(m_scoutUnit, m_scoutUnit->pos + (m_scoutUnit->pos - clusterPosition), m_bot);
	}
	return tooClose;
}

bool ScoutManager::attackEnemyCombat(std::vector<const sc2::Unit *> enemyUnitsInSight)
{
	bool attackingEnemy = false;
	const sc2::Unit * lowestHealthUnit;
	for (auto & unit : enemyUnitsInSight)
	{
		float dist = Util::Dist(unit->pos, m_scoutUnit->pos);
		if (Util::IsCombatUnit(unit, m_bot) && dist < Util::GetUnitTypeRange(m_scoutUnit->unit_type, m_bot) + 1) //+1 to be on the save side
		{
			if (attackingEnemy)
			{
				if (unit->health < lowestHealthUnit->health)
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
		sc2::AvailableAbilities abilities = m_bot.Query()->GetAbilitiesForUnit(m_scoutUnit);
		for (auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				m_bot.Actions()->UnitCommand(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, lowestHealthUnit);
				return attackingEnemy;
			}
		}
		
		Micro::SmartKiteTarget(m_scoutUnit, lowestHealthUnit, m_bot);
		
	}
	return attackingEnemy;
}

bool ScoutManager::attackEnemyWorker(std::vector<const sc2::Unit *> enemyUnitsInSight)
{
	bool attackingEnemy = false;
	const sc2::Unit * lowestHealthUnit;
	for (auto & unit : enemyUnitsInSight)
	{
		float dist = Util::Dist(unit->pos, m_scoutUnit->pos);
		if (Util::IsWorker(unit))
		{
			if (attackingEnemy)
			{
				if (unit->health < lowestHealthUnit->health)
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
		m_scoutStatus = "Found a victim (worker). Attacking!";
		sc2::AvailableAbilities abilities = m_bot.Query()->GetAbilitiesForUnit(m_scoutUnit);
		for (auto & ability : abilities.abilities)
		{
			if (ability.ability_id == sc2::ABILITY_ID::EFFECT_KD8CHARGE)
			{
				m_bot.Actions()->UnitCommand(m_scoutUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, lowestHealthUnit);
				return attackingEnemy;
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
	}
	return attackingEnemy;
}

bool ScoutManager::enemyWorkerInRadiusOf(const sc2::Point2D & pos) const
{
    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Enemy))
    {
        if (Util::IsWorker(unit) && Util::Dist(unit->pos, pos) < 10)
        {
            return true;
        }
    }

    return false;
}

void ScoutManager::scoutDamaged()
{
	auto scout = m_scoutUnit;
	if (Util::IsWorker(scout))
	{
		m_scoutStatus = "Too damaged. Retreating to base.";
		m_bot.Workers().finishedWithWorker(scout);
		m_numScouts = -1;
	}
	else
	{

		//Whos there in sight?
		std::vector<const sc2::Unit *> enemyUnitsInSight = getEnemyUnitsInSight(scout->pos);
		float sightDistance = Util::GetUnitTypeSight(m_scoutUnit->unit_type.ToType(), m_bot);
		if (enemyUnitsInSight.size()>0)
		{
			m_scoutStatus = "Too damaged. Fleeing...";
			sc2::Point2D RunningVector = scout->pos - Util::CalcCenter(enemyUnitsInSight);
			RunningVector *= (sightDistance +1) / std::sqrt(std::pow(RunningVector.x, 2) + pow(RunningVector.y, 2));

			Micro::SmartMove(scout, scout->pos + RunningVector,m_bot);
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
	//We use that it is ordered
	std::map<float,const BaseLocation *> allTargetBases;
	int numBasesEnemy = 0;
	for (auto & base : bases)
	{
		if (base->isOccupiedByPlayer(player))
		{
			m_bot.Debug()->DebugSphereOut(base->getMinerals().front()->pos, 3.0f, sc2::Colors::Red);
			numBasesEnemy++;
		}
		if (!(base->isOccupiedByPlayer(Players::Enemy)) && !(base->isOccupiedByPlayer(Players::Self)))
		{
			allTargetBases[base->getGroundDistance(pos)] = base;
		}
	}
	for (auto & base : allTargetBases)
	{
		if (m_targetBasesPositions.size() < numBasesEnemy || numBasesEnemy==0)
		{
			m_targetBasesPositions.push(base.second->getBasePosition());
		}
	}
}

const bool ScoutManager::dontBlowYourselfUp() const
{
	const sc2::Units grenades = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_KD8CHARGE));
	if (grenades.size() > 0)
	{
		for (auto & g : grenades)
		{
			if (Util::Dist(g->pos, m_scoutUnit->pos) < 4)
			{
				//escape 3/4 pi
				const float n = 1.0f / std::sqrt(2.0f);
				sc2::Point2D vector(g->pos.x - m_scoutUnit->pos.x, g->pos.y - m_scoutUnit->pos.y);
				vector.x = -n*(vector.x + vector.y);
				vector.y = n*(vector.x - vector.y);
				const sc2::Point2D targetPos = m_scoutUnit->pos + vector;
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

void ScoutManager::raiseAlarm(std::vector<const sc2::Unit *> enemyUnitsInSight)
{
	for (auto & enemy : enemyUnitsInSight)
	{
		if (m_bot.Data(enemy->unit_type).isBuilding)
		{
			m_foundProxy=true;
		}
	}
}