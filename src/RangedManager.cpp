#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"

RangedManager::RangedManager(CCBot & bot)
    : MicroManager(bot)
{

}

void RangedManager::executeMicro(const std::vector<const sc2::Unit *> & targets)
{
    assignTargets(targets);
}

void RangedManager::assignTargets(const std::vector<const sc2::Unit *> & targets)
{
    const std::vector<const sc2::Unit *> & rangedUnits = getUnits();

    // figure out targets
    std::vector<const sc2::Unit *> rangedUnitTargets;
    for (const auto & target : targets)
    {
        if (!target) { continue; }
        if (target->unit_type == sc2::UNIT_TYPEID::ZERG_EGG) { continue; }
        if (target->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA) { continue; }

        rangedUnitTargets.push_back(target);
    }
	//The idea is now to group the targets/targetPos
	std::unordered_map<const sc2::Unit *, sc2::Units > targetsAttackedBy;
	sc2::Units moveToPosition;
	//For the medivac we need either
	//Either the most injured
	std::map<float, const sc2::Unit *> injuredUnits;
	//Or the soldier in the front
	const sc2::Unit * frontSoldier = nullptr;
	sc2::Point2D orderPos = order.getPosition();
	float minDist = std::numeric_limits<float>::max();
	//Just checking if only medivacs available
	bool justMedivacs = true;
	for (const auto & injured : rangedUnits)
	{
		if (!m_bot.GetUnit(injured->tag) || !injured->is_alive)
		{
			//its too late
			continue;
		}
		if (injured->unit_type.ToType() != sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			justMedivacs = false;
		}
		else
		{
			continue;
		}
		auto attributes = m_bot.Observation()->GetUnitTypeData()[injured->unit_type].attributes;
		//We can only heal biological units
		if (std::find(attributes.begin(), attributes.end(), sc2::Attribute::Biological) != attributes.end())
		{
			float healthMissing = injured->health_max - injured->health;
			if (healthMissing>0)
			{
				injuredUnits[healthMissing] = injured;
			}
			float dist = Util::DistSq(injured->pos, orderPos);
			if (!frontSoldier || minDist > dist)
			{
				frontSoldier = injured;
				minDist = dist;
			}
		}
	}
	// In case it were really only medivacs
	if (justMedivacs)
	{
		Micro::SmartMove(rangedUnits, m_bot.Bases().getRallyPoint(),m_bot);
		return;
	}

	//Get effects like storm
	const std::vector<sc2::Effect> effects = m_bot.Observation()->GetEffects();

    // for each Unit
	auto test = m_bot.Observation()->GetEffectData()[12].radius;
    for (const auto & rangedUnit : rangedUnits)
    {
		//Don't stand in a storm etc
		bool fleeYouFools = false;
		for (const auto & effect : effects)
		{
			if (Util::isBadEffect(effect.effect_id))
			{
				const float radius = m_bot.Observation()->GetEffectData()[effect.effect_id].radius;
				for (const auto & pos : effect.positions)
				{
					Drawing::drawSphere(m_bot, pos, radius, sc2::Colors::Purple);
					if (Util::Dist(rangedUnit->pos, pos)<radius + 2.0f)
					{
						sc2::Point2D fleeingPos;
						if (effect.positions.size() == 1)
						{
							fleeingPos = pos + Util::normalizeVector(rangedUnit->pos - pos, radius + 2.0f);
						}
						else
						{
							const sc2::Point2D attackDirection = effect.positions.back() - effect.positions.front();
							fleeingPos = rangedUnit->pos + Util::normalizeVector(sc2::Point2D(-attackDirection.x,attackDirection.y), radius + 2.0f);
						}
						Micro::SmartMove(rangedUnit, fleeingPos, m_bot);
						fleeYouFools = true;
						break;
					}
				}
			}
			
			
			if (fleeYouFools)
			{
				break;
			}
		}
		if (fleeYouFools)
		{
			continue;
		}
        BOT_ASSERT(rangedUnit, "ranged unit is null");
        // if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			// find the best target for this rangedUnit
			//medivacs have the other ranged units as target.
			if (rangedUnit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				//find the nearest enemy
				const sc2::Unit * nearestEnemy = nullptr;
				float minDistTarget = std::numeric_limits<float>::max();
				for (const auto & target : rangedUnitTargets)
				{
					if (target->is_alive && Util::canHitMe(rangedUnit, target,m_bot))
					{
						float dist = Util::Dist(rangedUnit->pos, target->pos);
						if (!nearestEnemy || minDistTarget > dist)
						{
							nearestEnemy = target;
							minDistTarget = dist;
						}
					}
				}
				
				if (injuredUnits.size()>0)
				{
					const sc2::Unit* mostInjured = (injuredUnits.rbegin())->second;
					if (nearestEnemy && Util::Dist(rangedUnit->pos, nearestEnemy->pos) < Util::Dist(mostInjured->pos, nearestEnemy->pos))
					{
						m_bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS);
						sc2::Point2D targetPos = rangedUnit->pos;
						sc2::Point2D runningVector = mostInjured->pos - nearestEnemy->pos;
						runningVector *= (Util::GetAttackRange(rangedUnit->unit_type,m_bot) - 1) / (std::sqrt(Util::DistSq(runningVector)));
						targetPos += runningVector;
						Micro::SmartMove(rangedUnit, targetPos, m_bot);
					}
					else if (Util::Dist(rangedUnit->pos, mostInjured->pos) > 5)
					{
						m_bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS);
						if (rangedUnit->orders.empty() || rangedUnit->orders[0].target_unit_tag != mostInjured->tag)
						{
							m_bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::MOVE, mostInjured);
						}
					}
					else
					{
						if (rangedUnit->orders.empty() || rangedUnit->orders[0].ability_id != sc2::ABILITY_ID::EFFECT_HEAL)
						{
							m_bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::EFFECT_HEAL, mostInjured);
							injuredUnits.erase(std::prev(injuredUnits.end())); //no idea why rbegin is not working
						}
					}
				}
				else
				{
					if (rangedUnit->orders.empty() || frontSoldier && rangedUnit->orders[0].target_unit_tag && rangedUnit->orders[0].target_unit_tag != frontSoldier->tag)
					{
						m_bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::MOVE, frontSoldier);
					}
				}
			}
			else
			{
				if (!rangedUnitTargets.empty() || (order.getType() == SquadOrderTypes::Defend && Util::Dist(rangedUnit->pos, order.getPosition()) > 7))
				{
					const sc2::Unit * target = getTarget(rangedUnit, rangedUnitTargets);
					//if something goes wrong
					if (!target)
					{
						//This can happen with vikings
						if (Util::Dist(rangedUnit->pos, order.getPosition()) > 4)
						{
							// move to it
							Drawing::drawLine(m_bot,rangedUnit->pos, order.getPosition(), sc2::Colors::White);
							moveToPosition.push_back(rangedUnit);
						}
						continue;
					}
					if (order.getType() == SquadOrderTypes::Defend)
					{
						const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BUNKER }));
						if (Bunker.size() > 0 && Bunker.front()->cargo_space_taken != Bunker.front()->cargo_space_max)
						{
							if (Util::Dist(rangedUnit->pos, Bunker.front()->pos) < Util::Dist(rangedUnit->pos, target->pos))
							{
								Micro::SmartRightClick(rangedUnit, Bunker.front(), m_bot);
								m_bot.Actions()->UnitCommand(Bunker.front(), sc2::ABILITY_ID::LOAD, rangedUnit);
								continue;
							}
						}
					}
					//We only need fancy micro if we are in range and its not a building
					if (!m_bot.Data(target->unit_type).isBuilding && Util::Dist(rangedUnit->pos, target->pos) <= Util::GetAttackRange(rangedUnit->unit_type.ToType(), m_bot))
					{
						Micro::SmartKiteTarget(rangedUnit, target, m_bot);
					}
					//else we batch the attack comand first
					else
					{
						targetsAttackedBy[target].push_back(rangedUnit);
					}
				}
				// if there are no targets
				else
				{
					// if we're not near the order position
					if (Util::Dist(rangedUnit->pos, order.getPosition()) > 4)
					{
						// move to it
						Drawing::drawLine(m_bot,rangedUnit->pos, order.getPosition(), sc2::Colors::White);
						moveToPosition.push_back(rangedUnit);
					}
				}
			}
		}
    }
	//Grouped by target attack command
	for (const auto & t : targetsAttackedBy)
	{
		Micro::SmartAttackUnit(t.second, t.first, m_bot);
	}
	//Grouped by  position Move command
	if (moveToPosition.size() > 0)
	{
		Micro::SmartAttackMove(moveToPosition, order.getPosition(), m_bot);
	}
}

// get a target for the ranged unit to attack
const sc2::Unit * RangedManager::getTarget(const sc2::Unit * rangedUnit, const std::vector<const sc2::Unit *> & targets)
{
    BOT_ASSERT(rangedUnit, "null melee unit in getTarget");
    int highPriorityFar = 0;
	int highPriorityNear = 0;
    double closestDist = std::numeric_limits<double>::max();
	double lowestHealth = std::numeric_limits<double>::max();
    const sc2::Unit * closestTargetOutsideRange = nullptr;
	const sc2::Unit * weakestTargetInsideRange = nullptr;
	const float range = Util::GetAttackRange(rangedUnit->unit_type,m_bot);
    // for each target possiblity
	// We have three levels: in range, in sight, somewhere.
	// We want to attack the weakest/highest prio target in range
	// If there is no in range, we want to attack one in sight,
	// else the one with highest prio.
	for (const auto & targetUnit : targets)
	{
		BOT_ASSERT(targetUnit, "null target unit in getTarget");
		//Ignore dead units or ones we can not hit
		if (!targetUnit->is_alive || !Util::canHitMe(targetUnit,rangedUnit,m_bot))
		{
			continue;
		}
		int priority = getAttackPriority(rangedUnit, targetUnit);
		float distance = Util::Dist(rangedUnit->pos, targetUnit->pos);
		if (distance > range)
		{
			// If in sight we just add 20 to prio. This should make sure that a unit in sight has higher priority than any unit outside of range
			if (distance <= Util::GetUnitTypeSight(rangedUnit->unit_type, m_bot))
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
			if (!weakestTargetInsideRange || (priority > highPriorityNear) || (priority == highPriorityNear && targetUnit->health+targetUnit->shield < lowestHealth))
			{
				lowestHealth = targetUnit->health + targetUnit->shield;
				highPriorityNear = priority;
				weakestTargetInsideRange = targetUnit;
			}
		}

	}
    return weakestTargetInsideRange&&highPriorityNear>1 ? weakestTargetInsideRange: closestTargetOutsideRange;
}

// get the attack priority of a type in relation to a zergling
int RangedManager::getAttackPriority(const sc2::Unit * attacker, const sc2::Unit * unit)
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

