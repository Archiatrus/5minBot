#include "Micro.h"
#include "Util.h"
#include "CCBot.h"

const float dotRadius = 0.1f;

const sc2::Units CUnits2Units(CUnits units)
{
	sc2::Units output;
	for (const auto & unit : units)
	{
		output.push_back(unit->getUnit_ptr());
	}
	return output;
}


void Micro::SmartStop(CUnit_ptr attacker, CCBot & bot)
{
	BOT_ASSERT(attacker != nullptr, "Attacker is null");
	bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::STOP);
}

void Micro::SmartAttackUnit(CUnit_ptr attacker,CUnit_ptr target, CCBot & bot, bool queue)
{
	BOT_ASSERT(attacker != nullptr, "Attacker is null");
	BOT_ASSERT(target != nullptr, "Target is null");
	//if we are already attack it, we do not need to spam the attack
	if (!attacker->getOrders().empty() && attacker->getOrders().back().target_unit_tag == target->getTag())
	{
		return;
	}
	bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::ATTACK_ATTACK, target->getUnit_ptr(), queue);
}

void Micro::SmartAttackUnit(const CUnits & attacker, const CUnit_ptr target, CCBot & bot, bool queue)
{
	BOT_ASSERT(target != nullptr, "Target is null");
	//if we are already attack it, we do not need to spam the attack
	sc2::Units attackerThatNeedToAttack;
	sc2::Units attackerThatNeedToAttackMove;
	for (const auto & a : attacker)
	{
		if (!(a->getOrders().empty()) && (a->getOrders().back().target_unit_tag == target->getTag() || (a->getOrders().back().ability_id == sc2::ABILITY_ID::ATTACK && Util::Dist(a->getOrders().back().target_pos, target->getPos()) < 1.0f)))
		{
			continue;
		}
		//Distance to target
		//float dist = Util::Dist(a->getPos(), target->getPos());
		//Our range
		//float range = a->getAttackRange(target);
		if(target->isVisible())//if (a->getWeaponCooldown() == 0.0f && dist>range + 0.5f)
		{
			attackerThatNeedToAttack.push_back(a->getUnit_ptr());
		}
		else
		{
			attackerThatNeedToAttackMove.push_back(a->getUnit_ptr());
		}
	}
	if (attackerThatNeedToAttack.size() > 0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttack, sc2::ABILITY_ID::ATTACK_ATTACK, target->getUnit_ptr(), queue);
	}
	if (attackerThatNeedToAttackMove.size() > 0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttackMove, sc2::ABILITY_ID::ATTACK_ATTACK, target->getPos(), queue);
	}
}

void Micro::SmartAttackMove(CUnit_ptr attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
	BOT_ASSERT(attacker != nullptr, "Attacker is null");
	if (attacker->getOrders().empty() || attacker->getOrders().back().ability_id != sc2::ABILITY_ID::ATTACK || Util::Dist(attacker->getOrders().back().target_pos,targetPosition) > 1.0f)
	{
		bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::ATTACK_ATTACK, targetPosition);
	}
}

void Micro::SmartAttackMoveToUnit(CUnits & attacker, const CUnit_ptr target, CCBot & bot)
{
	sc2::Units attackerThatNeedToAttackTarget;
	sc2::Units attackerThatNeedToMove;
	sc2::Units attackerThatNeedToAttackMove;
	for (const auto & a : attacker)
	{
		const float range = a->getAttackRange(target);
		const float dist = Util::Dist(a->getPos(), target->getPos());
		if (dist <= range)
		{
			if (a->getOrders().empty() || a->getOrders().front().ability_id != sc2::ABILITY_ID::ATTACK || a->getOrders().front().target_unit_tag != target->getTag())
			{
				attackerThatNeedToAttackTarget.push_back(a->getUnit_ptr());
			}
		}
		else
		{
			if (a->getWeaponCooldown() > 0.0f)
			{
				if (a->getOrders().empty() || a->getOrders().front().ability_id != sc2::ABILITY_ID::MOVE || a->getOrders().front().target_pos != target->getPos())
				{
					attackerThatNeedToMove.push_back(a->getUnit_ptr());
				}
			}
			else
			{
				if (a->getOrders().empty() || a->getOrders().front().target_pos != target->getPos())
				{
				attackerThatNeedToAttackMove.push_back(a->getUnit_ptr());
				}
			}
		}
	}
	if (attackerThatNeedToAttackTarget.size()>0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttackTarget, sc2::ABILITY_ID::ATTACK_ATTACK, target->getUnit_ptr());
	}
	if (attackerThatNeedToMove.size()>0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToMove, sc2::ABILITY_ID::MOVE, target->getPos());
	}
	if (attackerThatNeedToAttackMove.size()>0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToAttackMove, sc2::ABILITY_ID::ATTACK, target->getPos());
	}
}


void Micro::SmartAttackMove(CUnits & attacker, const sc2::Point2D & targetPosition, CCBot & bot)
{
	sc2::Units attackerThatNeedToMove;
	for (const auto & a:attacker)
	{
		if (a->getOrders().empty() || a->getOrders().back().ability_id != sc2::ABILITY_ID::ATTACK || Util::Dist(a->getOrders().back().target_pos, targetPosition) > 1.0f)
		{
			attackerThatNeedToMove.push_back(a->getUnit_ptr());
		}
	}
	if (attackerThatNeedToMove.size()>0)
	{
		bot.Actions()->UnitCommand(attackerThatNeedToMove, sc2::ABILITY_ID::ATTACK, targetPosition);
	}
}

void Micro::SmartMove(const CUnits attackers,const CUnit_ptr target, CCBot & bot,const bool queue)
{
	sc2::Units attackersThatNeedToMove;
	for (const auto & attacker : attackers)
	{
		if (!attacker->getOrders().empty() && attacker->getOrders().back().ability_id == sc2::ABILITY_ID::MOVE && attacker->getOrders().back().target_unit_tag == target->getTag())
		{
			continue;
		}
		attackersThatNeedToMove.push_back(attacker->getUnit_ptr());
	}
	bot.Actions()->UnitCommand(attackersThatNeedToMove, sc2::ABILITY_ID::MOVE, target->getUnit_ptr(), queue);
}

void Micro::SmartMove(CUnit_ptr attacker, CUnit_ptr target, CCBot & bot, bool queue)
{
	//SmartMove(attacker, target->getPos(), bot, queue);
	if (!attacker->getOrders().empty() && attacker->getOrders().back().ability_id == sc2::ABILITY_ID::MOVE && attacker->getOrders().back().target_unit_tag == target->getTag())
	{
		return;
	}
	bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::MOVE, target->getUnit_ptr(), queue);
}

void Micro::SmartMove(CUnit_ptr attacker, const sc2::Point2D & targetPosition, CCBot & bot,bool queue)
{
	BOT_ASSERT(attacker != nullptr, "Attacker is null");
	//If we are already going there we do not have to spam it
	if (!attacker->getOrders().empty() && attacker->getOrders().back().ability_id == sc2::ABILITY_ID::MOVE && Util::Dist(attacker->getOrders().back().target_pos,targetPosition) < 0.1f || Util::Dist(attacker->getPos(),targetPosition) < 0.1f)
	{
		return;
	}
	if (!attacker->isFlying())
	{
		sc2::Point2D validWalkableTargetPosition = bot.Map().getClosestWalkableTo(targetPosition);
		bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::MOVE, validWalkableTargetPosition, queue);
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
			if (targetPosition.y > attacker->getPos().y)
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
			if (targetPosition.y > attacker->getPos().y)
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
			if (targetPosition.x > attacker->getPos().x)
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
			if (targetPosition.x > attacker->getPos().x)
			{
				targetPositionNew = sc2::Point2D(x_max, y_max);
			}
			else
			{
				targetPositionNew = sc2::Point2D(x_min, y_max);
			}
		}
		bot.Actions()->UnitCommand(attacker->getUnit_ptr(), sc2::ABILITY_ID::MOVE, targetPositionNew, queue);
	}
}

void Micro::SmartMove(CUnits attackers, const sc2::Point2D & targetPosition, CCBot & bot, bool queue)
{
	sc2::Point2D validWalkableTargetPosition = targetPosition;
	if (!(bot.Map().isWalkable(targetPosition) && bot.Map().isValid(targetPosition)))
	{
		sc2::Point2D attackersPos = Util::CalcCenter(attackers);
		sc2::Point2D homeVector = bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase() - attackersPos;
		homeVector *= Util::DistSq(attackersPos, targetPosition) / Util::DistSq(homeVector);
		validWalkableTargetPosition += homeVector;
	}

	sc2::Units flyingMover;
	sc2::Units walkingMover;
	for (const auto & attacker : attackers)
	{
		//If we are already going there we do not have to spam it
		if (!attacker->getOrders().empty() && attacker->getOrders().back().ability_id == sc2::ABILITY_ID::MOVE && Util::Dist(attacker->getOrders().back().target_pos, targetPosition) < 0.1f || Util::Dist(attacker->getPos(), targetPosition) < 0.1f)
		{
			continue;
		}
		if (!attacker->isFlying())
		{
			walkingMover.push_back(attacker->getUnit_ptr());
		}
		else
		{
			flyingMover.push_back(attacker->getUnit_ptr());
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

void Micro::SmartRightClick(CUnit_ptr unit, CUnit_ptr target, CCBot & bot, bool queue)
{
	BOT_ASSERT(unit != nullptr, "Unit is null");
	BOT_ASSERT(target != nullptr, "Unit is null");
	bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::SMART, target->getUnit_ptr(),queue);
}

void Micro::SmartRightClick(CUnits units, CUnit_ptr target, CCBot & bot, bool queue)
{
	BOT_ASSERT(units.size()>0, "Unit is null");
	BOT_ASSERT(target != nullptr, "Unit is null");
	bot.Actions()->UnitCommand(CUnits2Units(units), sc2::ABILITY_ID::SMART, target->getUnit_ptr(), queue);
}

void Micro::SmartRightClick(CUnit_ptr unit, CUnits targets, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit is null");
	BOT_ASSERT(targets.size()>0, "Unit is null");
	if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
	{
		CUnit_ptr target = nullptr;
		float minDist = std::numeric_limits<float>::max();
		for (const auto & t : targets)
		{
			float dist = Util::Dist(t->getPos(), unit->getPos());
			if (t->getLastSeenGameLoop()==unit->getLastSeenGameLoop() && (!target || minDist > dist))
			{
				minDist = dist;
				target = t;
			}
		}
		if (target)
		{
			bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::LOAD_MEDIVAC, target->getUnit_ptr());
		}
	}
	else
	{
		for (const auto & t : targets)
		{
			bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::SMART, t->getUnit_ptr());
		}
	}
}

void Micro::SmartRepair(CUnit_ptr unit, CUnit_ptr target, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Unit is null");
	bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::EFFECT_REPAIR, target->getUnit_ptr());
	if (!(target->isBuilding()))
	{
		bot.Actions()->UnitCommand(target->getUnit_ptr(), sc2::ABILITY_ID::MOVE, unit->getPos());
	}
}

void Micro::SmartKiteTarget(CUnit_ptr rangedUnit, CUnit_ptr target, CCBot & bot,bool queue)
{
	BOT_ASSERT(rangedUnit != nullptr, "RangedUnit is null");
	BOT_ASSERT(target != nullptr, "Target is null");
	//Distance to target
	float dist = Util::Dist(rangedUnit->getPos(), target->getPos());
	//Our range
	float range = rangedUnit->getAttackRange(target);
	if (rangedUnit->getWeaponCooldown() == 0.0f || dist>range+0.5f)
	{
		SmartAttackUnit(rangedUnit, target, bot,queue);
	}
	else
	{
		auto buffs = rangedUnit->getBuffs();
		if (rangedUnit->getHealth() == rangedUnit->getHealthMax() && (buffs.empty() || std::find(buffs.begin(), buffs.end(), sc2::BUFF_ID::STIMPACK) == buffs.end()))
		{
			sc2::AvailableAbilities abilities = rangedUnit->getAbilities();

			for (const auto & ability : abilities.abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::EFFECT_STIM)
				{
					bot.Actions()->UnitCommand(rangedUnit->getUnit_ptr(), sc2::ABILITY_ID::EFFECT_STIM);
					return;
				}
			}
		}
		//A normed vector to the target

		sc2::Point2D targetPos=rangedUnit->getPos();
		//If its a building we want range -1 distance
		//The same is true if it outranges us. We dont want to block following units
		if (target->isBuilding())
		{
			targetPos += Util::normalizeVector(target->getPos() - rangedUnit->getPos(), (dist - target->getRadius()) - (range - 1));
		}
		else if (target->getAttackRange(rangedUnit) >= range || (!rangedUnit->canHitMe(target) && rangedUnit->getUnitType().ToType()!=sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER))
		{
			targetPos += Util::normalizeVector(target->getPos() - rangedUnit->getPos(), (dist - target->getRadius()) - (range - 1));
		}
		else
		{
			targetPos += Util::normalizeVector(target->getPos() - rangedUnit->getPos(), (dist - target->getRadius()) - range);
		}
		
		SmartMove(rangedUnit, targetPos, bot, queue);
	}
}

void Micro::SmartBuild(CUnit_ptr builder, const sc2::UnitTypeID & buildingType, sc2::Point2D pos, CCBot & bot)
{
	BOT_ASSERT(builder != nullptr, "Builder is null");
	bot.Actions()->UnitCommand(builder->getUnit_ptr(), bot.Data(buildingType).buildAbility, pos);
}

void Micro::SmartBuildTarget(CUnit_ptr builder, const sc2::UnitTypeID & buildingType, CUnit_ptr target, CCBot & bot)
{
	BOT_ASSERT(builder != nullptr, "Builder is null");
	BOT_ASSERT(target != nullptr, "Target is null");
	bot.Actions()->UnitCommand(builder->getUnit_ptr(), bot.Data(buildingType).buildAbility, target->getUnit_ptr());
}

void Micro::SmartTrain(CUnit_ptr builder, const sc2::UnitTypeID & buildingType, CCBot & bot)
{
	BOT_ASSERT(builder != nullptr, "Builder is null");
	bot.Actions()->UnitCommand(builder->getUnit_ptr(), bot.Data(buildingType).buildAbility);
}

void Micro::SmartAbility(CUnit_ptr unit, const sc2::AbilityID & abilityID, CCBot & bot)
{
	BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->getOrders().empty() || unit->getOrders().back().ability_id != abilityID)
	{
		bot.Actions()->UnitCommand(unit->getUnit_ptr(), abilityID);
	}
}

void Micro::SmartAbility(CUnits units, const sc2::AbilityID & abilityID, CCBot & bot,bool queue)
{
		bot.Actions()->UnitCommand(CUnits2Units(units), abilityID,queue);
}

void Micro::SmartAbility(CUnit_ptr unit, const sc2::AbilityID & abilityID,const sc2::Point2D pos,CCBot & bot,bool queue)
{
	BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->getOrders().empty() || unit->getOrders().back().ability_id != abilityID || unit->getOrders().back().target_pos.x != pos.x || unit->getOrders().back().target_pos.y != pos.y)
	{
		bot.Actions()->UnitCommand(unit->getUnit_ptr(), abilityID,pos,queue);
	}
}

void Micro::SmartAbility(CUnit_ptr unit, const sc2::AbilityID & abilityID, CUnit_ptr target, CCBot & bot, bool queue)
{
	BOT_ASSERT(unit != nullptr, "Builder is null");
	if (unit->getOrders().empty() || unit->getOrders().back().ability_id != abilityID || unit->getOrders().back().target_unit_tag != target->getTag())
	{
		bot.Actions()->UnitCommand(unit->getUnit_ptr(), abilityID, target->getUnit_ptr(), queue);
	}
}


void Micro::SmartCDAbility(CUnit_ptr builder, const sc2::AbilityID & abilityID, CCBot & bot, bool queue)
{
	BOT_ASSERT(builder != nullptr, "Builder is null");
	if (builder->getAbilityCoolDown() <= bot.Observation()->GetGameLoop())
	{
		sc2::AvailableAbilities abilities = builder->getAbilities();
		for (const auto & ability : abilities.abilities)
		{
			if (ability.ability_id == abilityID)
			{
				builder->newAbilityCoolDown(bot.Observation()->GetGameLoop() + bot.Data(abilityID));
				bot.Actions()->UnitCommand(builder->getUnit_ptr(), abilityID, queue);
				return;
			}
		}
	}
}

void Micro::SmartCDAbility(CUnits units, const sc2::AbilityID & abilityID, CCBot & bot, bool queue)
{
	sc2::Units targets;
	for (const auto & unit : units)
	{
		if (unit->getAbilityCoolDown() <= bot.Observation()->GetGameLoop())
		{
			for (const auto & ability : unit->getAbilities().abilities)
			{
				if (ability.ability_id == abilityID)
				{
					unit->newAbilityCoolDown(bot.Observation()->GetGameLoop()+bot.Data(abilityID));
					targets.push_back(unit->getUnit_ptr());
					continue;
				}
			}
		}
	}
	bot.Actions()->UnitCommand(targets, abilityID,queue);
	return;
}

void Micro::SmartStim(CUnits units, CCBot & bot, bool queue)
{
	sc2::Units targets;
	for (const auto & unit : units)
	{
		auto buffs = unit->getBuffs();
		if (unit->getHealth() == unit->getHealthMax() && (buffs.empty() || std::find(buffs.begin(), buffs.end(), sc2::BUFF_ID::STIMPACK) == buffs.end()))
		{
			sc2::AvailableAbilities abilities = unit->getAbilities();

			for (const auto & ability : abilities.abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::EFFECT_STIM)
				{
					targets.push_back(unit->getUnit_ptr());
					continue;
				}
			}
		}
	}
	bot.Actions()->UnitCommand(targets, sc2::ABILITY_ID::EFFECT_STIM,queue);
}