#include "SiegeManager.h"
#include "Util.h"
#include "CCBot.h"

SiegeManager::SiegeManager(CCBot & bot)
	: MicroManager(bot)
{

}

void SiegeManager::executeMicro(const CUnits & targets)
{
	assignTargets(targets);
}

void SiegeManager::assignTargets(const CUnits & targets)
{
	const CUnits & SiegeUnits = getUnits();

	// figure out targets
	CUnits SiegeUnitTargets;
	for (const auto & target : targets)
	{
		if (!target) { continue; }
		if (target->isFlying()) { continue; }
		if (target->getUnitType() == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
		if (target->getUnitType() == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }

		SiegeUnitTargets.push_back(target);
	}

	// for each Unit
	for (const auto & siegeUnit : SiegeUnits)
	{
		BOT_ASSERT(siegeUnit, "melee unit is null");
		if (siegeUnit->getUnit_ptr()->is_selected)
		{
			int a = 1;
		}
		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			// if there are targets
			if (!SiegeUnitTargets.empty())
			{
				// find the best target for this Unit
				const CUnit_ptr target = getTarget(siegeUnit, SiegeUnitTargets);
				//if we have a target
				if (target)
				{
					//If the tank is in mobile mode
					if (siegeUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANK)
					{
						if (Util::Dist(siegeUnit->getPos(), target->getPos()) <= 13.0f)
						{
							Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE, m_bot);
						}
						else
						{
							Micro::SmartAttackMove(siegeUnit, target->getPos(), m_bot);
						}
					}
					//if we are stationary
					else
					{
						//Attack if near enough
						const float dist = Util::Dist(siegeUnit->getPos(), target->getPos());
						const float range = 13.0;// siegeUnit->getAttackRange(target);
						if (dist <= range)
						{
							Micro::SmartAttackUnit(siegeUnit, target, m_bot);
						}
						//go to mobile mode
						else if (dist > range + 2.0f)
						{
							Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE, m_bot);
						}
					}
				}
				//if we have no target
				else
				{
					// if we're not near the order position
					if (Util::Dist(siegeUnit->getPos(), order.getPosition()) > 4)
					{
						// move to it
						if (siegeUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
						{
							Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE, m_bot);
						}
						else
						{
							Micro::SmartAttackMove(siegeUnit, order.getPosition(), m_bot);
						}
					}
					else
					{
						if (siegeUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANK)
						{
							Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE,m_bot);
						}
					}
				}
			}
			// if there are no targets
			else
			{
				// if we're not near the order position
				if (Util::Dist(siegeUnit->getPos(), order.getPosition()) > 4)
				{
					// move to it
					if (siegeUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
					{
						Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE, m_bot);
					}
					else
					{
						Micro::SmartAttackMove(siegeUnit, order.getPosition(), m_bot);
					}
				}
				else
				{
					if (siegeUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANK)
					{
						Micro::SmartAbility(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE, m_bot);
					}
				}
			}
		}
	}
}

// get a target for the meleeUnit to attack
const CUnit_ptr SiegeManager::getTarget(const CUnit_ptr siegeUnit, const CUnits & targets)
{
	BOT_ASSERT(siegeUnit, "null melee unit in getTarget");
	int highPriorityFar = 0;
	int highPriorityNear = 0;
	double closestDist = std::numeric_limits<double>::max();
	double lowestHealth = std::numeric_limits<double>::max();
	CUnit_ptr closestTargetOutsideRange = nullptr;
	CUnit_ptr weakestTargetInsideRange = nullptr;
	const float range = 13.0; //We want the sieged range
	// for each target possiblity
	// We have three levels: in range, in sight, somewhere.
	// We want to attack the weakest/highest prio target in range
	// If there is no in range, we want to attack one in sight,
	// else the one with highest prio.
	for (const auto & targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getTarget");
		//Ignore dead units or ones we can not hit
		if (!targetUnit->isAlive() || !targetUnit->canHitMe(siegeUnit))
		{
			continue;
		}
		int priority = getAttackPriority(siegeUnit, targetUnit);
		float distance = Util::Dist(siegeUnit->getPos(), targetUnit->getPos());
		//Tanks have a minimum range
		if (distance < 2.0f)
		{
			continue;
		}

		if (distance > range)
		{
			// If in sight we just add 20 to prio. This should make sure that a unit in sight has higher priority than any unit outside of range
			if (distance <= siegeUnit->getSightRange())
			{
				priority += 20;
			}
			// if it's a higher priority, or it's closer, set it
			if (!closestTargetOutsideRange || (priority > highPriorityFar) || (priority == highPriorityFar && distance < closestDist))
			{
				closestDist = distance;
				highPriorityFar = priority;
				closestTargetOutsideRange = targetUnit;
			}
		}
		else
		{
			if (!weakestTargetInsideRange || (priority > highPriorityNear) || (priority == highPriorityNear && targetUnit->getHealth() < lowestHealth))
			{
				lowestHealth = targetUnit->getHealth();
				highPriorityNear = priority;
				weakestTargetInsideRange = targetUnit;
			}
		}

	}
	return weakestTargetInsideRange&&highPriorityNear>1 ? weakestTargetInsideRange : closestTargetOutsideRange;
}

// get the attack priority of a type in relation to a zergling
int SiegeManager::getAttackPriority(const CUnit_ptr attacker, const CUnit_ptr unit)
{
	BOT_ASSERT(unit, "null unit in getAttackPriority");

	if (unit->isCombatUnit())
	{
		if (unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_BANELING)
		{
			return 11;
		}
		return 10;
	}
	if (unit->getUnitType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER)
	{
		return 10;
	}
	if (unit->isWorker())
	{
		return 10;
	}
	if (unit->getUnitType() == sc2::UNIT_TYPEID::PROTOSS_PYLON || unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
	{
		return 5;
	}
	if (Util::IsTownHallType(unit->getUnitType()))
	{
		return 4;
	}
	return 1;
}
