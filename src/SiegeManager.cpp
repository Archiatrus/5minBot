#include "SiegeManager.h"
#include "Util.h"
#include "CCBot.h"

SiegeManager::SiegeManager(CCBot & bot)
	: MicroManager(bot)
{

}

void SiegeManager::executeMicro(const std::vector<const sc2::Unit *> & targets)
{
	assignTargets(targets);
}

void SiegeManager::assignTargets(const std::vector<const sc2::Unit *> & targets)
{
	const std::vector<const sc2::Unit *> & SiegeUnits = getUnits();

	// figure out targets
	std::vector<const sc2::Unit *> SiegeUnitTargets;
	for (const auto & target : targets)
	{
		if (!target) { continue; }
		if (target->is_flying) { continue; }
		if (target->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
		if (target->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }

		SiegeUnitTargets.push_back(target);
	}

	// for each Unit
	for (const auto & siegeUnit : SiegeUnits)
	{
		BOT_ASSERT(siegeUnit, "melee unit is null");

		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			// if there are targets
			if (!SiegeUnitTargets.empty())
			{
				// find the best target for this Unit
				const sc2::Unit * target = getTarget(siegeUnit, SiegeUnitTargets);
				//if we have a target
				if (target)
				{
					//If the tank is in mobile mode
					if (siegeUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK)
					{
						if (Util::Dist(siegeUnit->pos, target->pos) <= 11.0f)
						{
							if (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_SIEGEMODE)
							{
								m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE);
							}
						}
						else
						{
							Micro::SmartAttackMove(siegeUnit, target->pos, m_bot);
						}
					}
					//if we are stationary
					else
					{
						//Attack if near enough
						if (Util::Dist(siegeUnit->pos, target->pos) <= 15.0f)
						{
							Micro::SmartAttackUnit(siegeUnit, target, m_bot);
						}
						//go to mobile mode
						else
						{
							if (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_UNSIEGE)
							{
								m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE);
							}
						}
					}
				}
				//if we have no target
				else
				{
					// if we're not near the order position
					if (Util::Dist(siegeUnit->pos, order.getPosition()) > 4)
					{
						// move to it
						if (siegeUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
						{
							if (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_UNSIEGE)
							{
								m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE);
							}
						}
						else
						{
							Micro::SmartAttackMove(siegeUnit, order.getPosition(), m_bot);
						}
					}
					else
					{
						if (siegeUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK && (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_SIEGEMODE))
						{
							m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE);
						}
					}
				}
			}
			// if there are no targets
			else
			{
				// if we're not near the order position
				if (Util::Dist(siegeUnit->pos, order.getPosition()) > 4)
				{
					// move to it
					if (siegeUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
					{
						if (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_UNSIEGE)
						{
							m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_UNSIEGE);
						}
					}
					else
					{
						Micro::SmartAttackMove(siegeUnit, order.getPosition(), m_bot);
					}
				}
				else
				{
					if (siegeUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK && (siegeUnit->orders.empty() || siegeUnit->orders.back().ability_id != sc2::ABILITY_ID::MORPH_SIEGEMODE))
					{
						m_bot.Actions()->UnitCommand(siegeUnit, sc2::ABILITY_ID::MORPH_SIEGEMODE);
					}
				}
			}
		}

		if (m_bot.Config().DrawUnitTargetInfo)
		{
			// TODO: draw the line to the unit's target
		}
	}
}

// get a target for the meleeUnit to attack
const sc2::Unit * SiegeManager::getTarget(const sc2::Unit * siegeUnit, const std::vector<const sc2::Unit *> & targets)
{
	BOT_ASSERT(siegeUnit, "null melee unit in getTarget");
	int highPriorityFar = 0;
	int highPriorityNear = 0;
	double closestDist = std::numeric_limits<double>::max();
	double lowestHealth = std::numeric_limits<double>::max();
	const sc2::Unit * closestTargetOutsideRange = nullptr;
	const sc2::Unit * weakestTargetInsideRange = nullptr;
	const float range = 13; //We want the sieged range
	// for each target possiblity
	// We have three levels: in range, in sight, somewhere.
	// We want to attack the weakest/highest prio target in range
	// If there is no in range, we want to attack one in sight,
	// else the one with highest prio.
	for (const auto & targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getTarget");
		//Ignore dead units or ones we can not hit
		if (!targetUnit->is_alive || !Util::canHitMe(targetUnit, siegeUnit, m_bot))
		{
			continue;
		}
		int priority = getAttackPriority(siegeUnit, targetUnit);
		float distance = Util::Dist(siegeUnit->pos, targetUnit->pos);
		//Tanks have a minimum range
		if (distance < 2.0f)
		{
			continue;
		}

		if (distance > range)
		{
			// If in sight we just add 20 to prio. This should make sure that a unit in sight has higher priority than any unit outside of range
			if (distance <= Util::GetUnitTypeSight(siegeUnit->unit_type, m_bot))
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
			if (!weakestTargetInsideRange || (priority > highPriorityNear) || (priority == highPriorityNear && targetUnit->health + targetUnit->shield < lowestHealth))
			{
				lowestHealth = targetUnit->health + targetUnit->shield;
				highPriorityNear = priority;
				weakestTargetInsideRange = targetUnit;
			}
		}

	}
	return weakestTargetInsideRange&&highPriorityNear>1 ? weakestTargetInsideRange : closestTargetOutsideRange;
}

// get the attack priority of a type in relation to a zergling
int SiegeManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * unit)
{
	BOT_ASSERT(unit, "null unit in getAttackPriority");

	if (Util::IsCombatUnit(unit, m_bot))
	{
		if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_BANELING)
		{
			return 11;
		}
		return 10;
	}
	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER)
	{
		return 10;
	}
	if (Util::IsWorker(unit))
	{
		return 10;
	}
	if (unit->unit_type == sc2::UNIT_TYPEID::PROTOSS_PYLON || unit->unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
	{
		return 5;
	}
	if (Util::IsTownHallType(unit->unit_type))
	{
		return 4;
	}
	return 1;
}
