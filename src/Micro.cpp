#include "Micro.h"
#include "Util.h"
#include "CCBot.h"


const float dotRadius = 0.1f;

void Micro::SmartStop(const sc2::Unit * attacker, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
    bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::STOP);
}

void Micro::SmartAttackUnit(const sc2::Unit * attacker, const sc2::Unit * target, CCBot & bot, bool queue)
{
	BOT_ASSERT(attacker != nullptr, "Attacker is null");
	BOT_ASSERT(target != nullptr, "Target is null");
	//if we are already attack it, we do not need to spam the attack
	if (!attacker->orders.empty() && attacker->orders.back().target_unit_tag == target->tag)
	{
		return;
	}
	bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK_ATTACK, target, queue);
}

void Micro::SmartAttackUnit(const sc2::Units & attacker, const sc2::Unit * target, CCBot & bot, bool queue)
{
	BOT_ASSERT(target != nullptr, "Target is null");
	//if we are already attack it, we do not need to spam the attack
	sc2::Units attackerThatNeedToAttack;
	sc2::Units attackerThatNeedToAttackMove;
	for (const auto & a : attacker)
	{
		if (!(a->orders.empty()) && a->orders.back().target_unit_tag == target->tag)
		{
			continue;
		}
		//Distance to target
		float dist = Util::Dist(a->pos, target->pos);
		//Our range
		float range = Util::GetAttackRange(a->unit_type, bot);
		if (a->weapon_cooldown == 0.0f && dist>range + 0.5f)
		{
			Drawing::drawLine(bot,a->pos, target->pos, sc2::Colors::Yellow);
			attackerThatNeedToAttackMove.push_back(a);
		}
		else
		{
			Drawing::drawLine(bot,a->pos, target->pos, sc2::Colors::Red);
			attackerThatNeedToAttack.push_back(a);
		}
	}
	if (attackerThatNeedToAttack.size() > 0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttack, sc2::ABILITY_ID::ATTACK_ATTACK, target, queue);
	}
	if (attackerThatNeedToAttackMove.size() > 0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttackMove, sc2::ABILITY_ID::ATTACK_ATTACK, target->pos, queue);
	}
}

void Micro::SmartAttackMove(const sc2::Unit * attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
	if (attacker->orders.empty() || attacker->orders.back().ability_id != sc2::ABILITY_ID::ATTACK_ATTACK || Util::Dist(attacker->orders.back().target_pos,targetPosition) > 1.0f)
	{
		bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK_ATTACK, targetPosition);
	}
}

void Micro::SmartAttackMove(const sc2::Units & attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
	sc2::Units attackerThatNeedToMove;
	for (const auto & a:attacker)
	{
		if (a->orders.empty() || a->orders.back().ability_id != sc2::ABILITY_ID::ATTACK_ATTACK || Util::Dist(a->orders.back().target_pos, targetPosition) > 1.0f)
		{
			attackerThatNeedToMove.push_back(a);
		}
	}
	if (attackerThatNeedToMove.size()>0)
	{
		bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK_ATTACK, targetPosition);
	}
}

void Micro::SmartMove(const sc2::Unit * attacker, const sc2::Point2D & targetPosition, CCBot & bot,bool queue)
{
    BOT_ASSERT(attacker != nullptr, "Attacker is null");
	//If we are already going there we do not have to spam it
	if (!attacker->orders.empty() && attacker->orders.back().ability_id == sc2::ABILITY_ID::MOVE && Util::Dist(attacker->orders.back().target_pos,targetPosition) < 0.1f || Util::Dist(attacker->pos,targetPosition) < 0.1f)
	{
		return;
	}
	if (!attacker->is_flying)
	{
		sc2::Point2D validWalkableTargetPosition = targetPosition;
		if (!(bot.Map().isWalkable(targetPosition) && bot.Map().isValid(targetPosition)))
		{
			sc2::Point2D homeVector = bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition() - attacker->pos;
			homeVector *= Util::DistSq(attacker->pos, targetPosition) / Util::DistSq(homeVector);
			validWalkableTargetPosition += homeVector;
		}
		bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::MOVE, validWalkableTargetPosition, queue);
	}
	else
	{
		sc2::Point2D targetPositionNew = targetPosition;
		float x_min = static_cast<float>(bot.Observation()->GetGameInfo().playable_min.x);
		float x_max = static_cast<float>(bot.Observation()->GetGameInfo().playable_max.x);
		float y_min = static_cast<float>(bot.Observation()->GetGameInfo().playable_min.y);
		float y_max = static_cast<float>(bot.Observation()->GetGameInfo().playable_max.y);

		if (targetPosition.x < x_min)
		{
			if (targetPosition.y > attacker->pos.y)
			{
				targetPositionNew = sc2::Point2D(x_min, y_max);
			}
			else
			{
				targetPositionNew = sc2::Point2D(x_min, y_min);
			}
		}
		else if (targetPosition.x > x_max)
		{
			if (targetPosition.y > attacker->pos.y)
			{
				targetPositionNew = sc2::Point2D(x_max, y_max);
			}
			else
			{
				targetPositionNew = sc2::Point2D(x_max, y_min);
			}
		}
		else if (targetPosition.y < y_min)
		{
			if (targetPosition.x > attacker->pos.x)
			{
				targetPositionNew = sc2::Point2D(x_max, y_min);
			}
			else
			{
				targetPositionNew = sc2::Point2D(x_min, y_min);
			}
		}
		else if (targetPosition.y > y_max)
		{
			if (targetPosition.x > attacker->pos.x)
			{
				targetPositionNew = sc2::Point2D(x_max, y_max);
			}
			else
			{
				targetPositionNew = sc2::Point2D(x_min, y_max);
			}
		}
		bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::MOVE, targetPositionNew, queue);
	}
}

void Micro::SmartMove(sc2::Units attackers, const sc2::Point2D & targetPosition, CCBot & bot, bool queue)
{
	sc2::Point2D validWalkableTargetPosition = targetPosition;
	if (!(bot.Map().isWalkable(targetPosition) && bot.Map().isValid(targetPosition)))
	{
		sc2::Point2D attackersPos = Util::CalcCenter(attackers);
		sc2::Point2D homeVector = bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getPosition() - attackersPos;
		homeVector *= Util::DistSq(attackersPos, targetPosition) / Util::DistSq(homeVector);
		validWalkableTargetPosition += homeVector;
	}

	sc2::Units flyingMover;
	sc2::Units walkingMover;
	for (const auto & attacker : attackers)
	{
		//If we are already going there we do not have to spam it
		if (!attacker->orders.empty() && attacker->orders.back().ability_id == sc2::ABILITY_ID::MOVE && Util::Dist(attacker->orders.back().target_pos, targetPosition) < 0.1f || Util::Dist(attacker->pos, targetPosition) < 0.1f)
		{
			continue;
		}
		if (!attacker->is_flying)
		{
			walkingMover.push_back(attacker);
		}
		else
		{
			flyingMover.push_back(attacker);
		}
	}
	if (walkingMover.size() > 0)
	{
		bot.Actions()->UnitCommand(walkingMover, sc2::ABILITY_ID::MOVE, validWalkableTargetPosition, queue);
	}
	if (flyingMover.size() > 0)
	{
		bot.Actions()->UnitCommand(flyingMover, sc2::ABILITY_ID::MOVE, targetPosition, queue);
	}
}

void Micro::SmartRightClick(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot, bool queue)
{
    BOT_ASSERT(unit != nullptr, "Unit is null");
	BOT_ASSERT(target != nullptr, "Unit is null");
    bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, target,queue);
}

void Micro::SmartRightClick(sc2::Units units, const sc2::Unit * target, CCBot & bot, bool queue)
{
	BOT_ASSERT(units.size()>0, "Unit is null");
	BOT_ASSERT(target != nullptr, "Unit is null");
	bot.Actions()->UnitCommand(units, sc2::ABILITY_ID::SMART, target, queue);
}

void Micro::SmartRightClick(const sc2::Unit * unit, sc2::Units targets, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit is null");
	BOT_ASSERT(targets.size()>0, "Unit is null");
	if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
	{
		const sc2::Unit * target = nullptr;
		float minDist = std::numeric_limits<float>::max();
		for (const auto & t : targets)
		{
			float dist = Util::Dist(t->pos, unit->pos);
			if (t->last_seen_game_loop==unit->last_seen_game_loop && (!target || minDist > dist))
			{
				minDist = dist;
				target = t;
			}
		}
		if (target)
		{
			bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::LOAD_MEDIVAC, target);
		}
	}
	else
	{
		for (const auto & t : targets)
		{
			bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, t, true);
		}
	}
}

void Micro::SmartRepair(const sc2::Unit * unit, const sc2::Unit * target, CCBot & bot)
{
    BOT_ASSERT(unit != nullptr, "Unit is null");
    bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::EFFECT_REPAIR, target);
}

void Micro::SmartKiteTarget(const sc2::Unit * rangedUnit, const sc2::Unit * target, CCBot & bot,bool queue)
{
    BOT_ASSERT(rangedUnit != nullptr, "RangedUnit is null");
    BOT_ASSERT(target != nullptr, "Target is null");
	//Distance to target
	float dist = Util::Dist(rangedUnit->pos, target->pos);
	//Our range
	float range = Util::GetAttackRange(rangedUnit->unit_type, bot);
	if (rangedUnit->weapon_cooldown == 0.0f || dist>range+0.5f)
	{
		SmartAttackUnit(rangedUnit, target, bot,queue);
	}
	else
	{
		auto buffs = rangedUnit->buffs;
		if (rangedUnit->health == rangedUnit->health_max && (buffs.empty() || std::find(buffs.begin(), buffs.end(), sc2::BUFF_ID::STIMPACK) == buffs.end()))
		{
			sc2::AvailableAbilities abilities = bot.Query()->GetAbilitiesForUnit(rangedUnit);

			for (const auto & ability : abilities.abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::EFFECT_STIM)
				{
					bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::EFFECT_STIM);
					return;
				}
			}
		}
		//A normed vector to the target
		sc2::Point2D RunningVector = target->pos - rangedUnit->pos;
		RunningVector /=  std::sqrt(std::pow(RunningVector.x, 2) + pow(RunningVector.y, 2));

		sc2::Point2D targetPos=rangedUnit->pos;
		//If its a building we want range -1 distance
		//The same is true if it outranges us. We dont want to block following units
		if (bot.Data(target->unit_type).isBuilding)
		{
			targetPos += (dist - (range - 1))*RunningVector;
		}
		else if (Util::GetAttackRange(target->unit_type, bot) >= range || (!Util::canHitMe(rangedUnit,target,bot) && rangedUnit->unit_type.ToType()!=sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER))
		{
			targetPos = target->pos;
		}
		else
		{
			targetPos += (dist - range)*RunningVector;
		}

		SmartMove(rangedUnit, targetPos, bot, queue);
	}
}

void Micro::SmartBuild(const sc2::Unit * builder, const sc2::UnitTypeID & buildingType, sc2::Point2D pos, CCBot & bot)
{
    BOT_ASSERT(builder != nullptr, "Builder is null");
    bot.Actions()->UnitCommand(builder, bot.Data(buildingType).buildAbility, pos);
}

void Micro::SmartBuildTarget(const sc2::Unit * builder, const sc2::UnitTypeID & buildingType, const sc2::Unit * target, CCBot & bot)
{
    BOT_ASSERT(builder != nullptr, "Builder is null");
    BOT_ASSERT(target != nullptr, "Target is null");
    bot.Actions()->UnitCommand(builder, bot.Data(buildingType).buildAbility, target);
}

void Micro::SmartTrain(const sc2::Unit * builder, const sc2::UnitTypeID & buildingType, CCBot & bot)
{
    BOT_ASSERT(builder != nullptr, "Builder is null");
    bot.Actions()->UnitCommand(builder, bot.Data(buildingType).buildAbility);
}

void Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, CCBot & bot)
{
    BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->orders.empty() || unit->orders.back().ability_id != abilityID)
	{
		bot.Actions()->UnitCommand(unit, abilityID);
	}
}

void Micro::SmartAbility(sc2::Units units, const sc2::AbilityID & abilityID, CCBot & bot,bool queue)
{
		bot.Actions()->UnitCommand(units, abilityID);
}

void Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID,const sc2::Point2D pos,CCBot & bot,bool queue)
{
	BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->orders.empty() || unit->orders.back().ability_id != abilityID || unit->orders.back().target_pos.x != pos.x || unit->orders.back().target_pos.y != pos.y)
	{
		bot.Actions()->UnitCommand(unit, abilityID,pos,queue);
	}
}

void Micro::SmartAbility(const sc2::Unit * unit, const sc2::AbilityID & abilityID, const sc2::Unit * target, CCBot & bot, bool queue)
{
	BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->orders.empty() || unit->orders.back().ability_id != abilityID || unit->orders.back().target_unit_tag != target->tag)
	{
		bot.Actions()->UnitCommand(unit, abilityID, target, queue);
	}
}


void Micro::SmartCDAbility(const sc2::Unit * builder, const sc2::AbilityID & abilityID, CCBot & bot, bool queue)
{
	BOT_ASSERT(builder != nullptr, "Builder is null");
	sc2::AvailableAbilities abilities = bot.Query()->GetAbilitiesForUnit(builder);

	for (const auto & ability : abilities.abilities)
	{
		if (ability.ability_id == abilityID)
		{
			bot.Actions()->UnitCommand(builder, abilityID,queue);
			return;
		}
	}
}

void Micro::SmartCDAbility(sc2::Units units, const sc2::AbilityID & abilityID, CCBot & bot, bool queue)
{
	sc2::Units targets;
	for (const auto & unit : units)
	{
		sc2::AvailableAbilities abilities = bot.Query()->GetAbilitiesForUnit(unit);

		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == abilityID)
			{
				targets.push_back(unit);
				continue;
			}
		}
	}
	bot.Actions()->UnitCommand(targets, abilityID,queue);
	return;
}

void Micro::SmartStim(sc2::Units units, CCBot & bot, bool queue)
{
	sc2::Units targets;
	for (const auto & unit : units)
	{
		auto buffs = unit->buffs;
		if (unit->health == unit->health_max && (buffs.empty() || std::find(buffs.begin(), buffs.end(), sc2::BUFF_ID::STIMPACK) == buffs.end()))
		{
			sc2::AvailableAbilities abilities = bot.Query()->GetAbilitiesForUnit(unit);

			for (const auto & ability : abilities.abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::EFFECT_STIM)
				{
					targets.push_back(unit);
					continue;
				}
			}
		}
	}
	bot.Actions()->UnitCommand(targets, sc2::ABILITY_ID::EFFECT_STIM);
}