#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"

uint32_t counter;
const uint32_t speedFactor = 10;

RangedManager::RangedManager(CCBot & bot)
	: MicroManager(bot)
{
	counter = 0;
}

void RangedManager::executeMicro(const CUnits & targets)
{
	++counter;
	assignTargets(targets);
}

void RangedManager::assignTargets(const CUnits & targetsRaw)
{
	// figure out targets
	CUnits rangedUnitTargets;
	for (const auto & target : targetsRaw)
	{
		if (!target) { continue; }
		if (!target->isVisible() && !target->isTownHall()) { continue; }
		if (target->isType(sc2::UNIT_TYPEID::ZERG_EGG)) { continue; }
		if (target->isType(sc2::UNIT_TYPEID::ZERG_LARVA)) { continue; }
		if (target->getPos() == sc2::Point3D()) { continue; }
		rangedUnitTargets.push_back(target);
	}
	const auto sortedUnitTargets = getAttackPriority(rangedUnitTargets);
	// The idea is now to group the targets/targetPos
	std::unordered_map<CUnit_ptr, CUnits> targetsAttackedBy;
	std::unordered_map<CUnit_ptr, CUnits> targetsMovedTo;
	CUnits moveToPosition;
	// For the medivac we need either
	// Either the most injured
	std::map<float, CUnit_ptr> injuredUnits;
	// Or the soldier in the front
	CUnit_ptr frontSoldier = nullptr;
	sc2::Point2D orderPos = order.getPosition();
	float minDist = std::numeric_limits<float>::max();
	// Just checking if only medivacs available
	bool justMedivacs = true;
	const CUnits & rangedUnits = getUnits();
	// Being healed is not a buff. So we need to check every medivac if the injured unit is already healed.
	const CUnits medivacs = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_MEDIVAC);
	for (const auto & injured : rangedUnits)
	{
		if (!injured->isAlive())
		{
			// its too late
			continue;
		}
		if (injured->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			justMedivacs = false;
		}
		else
		{
			continue;
		}
		// We can only heal biological units
		if (injured->hasAttribute(sc2::Attribute::Biological))
		{
			const float dist = Util::DistSq(injured->getPos(), orderPos);
			if (!frontSoldier || minDist > dist)
			{
				frontSoldier = injured;
				minDist = dist;
			}
			const float healthMissing = injured->getHealthMax() - injured->getHealth();
			if (healthMissing > 0.0f)
			{
				bool isHealing = false;
				for (const auto & medivac : medivacs)
				{
					if (!medivac->getOrders().empty() && medivac->getOrders().front().ability_id == sc2::ABILITY_ID::EFFECT_HEAL && medivac->getOrders().front().target_unit_tag == injured->getTag())
					{
						isHealing = true;
						break;
					}
				}
				if (!isHealing)
				{
					injuredUnits[healthMissing] = injured;
				}
			}
		}
	}
	// In case it were really only medivacs
	if (justMedivacs)
	{
		Micro::SmartMove(rangedUnits, m_bot.Bases().getRallyPoint(), m_bot);
		return;
	}

	// Get effects like storm
	const std::vector<sc2::Effect> effects = m_bot.Observation()->GetEffects();

	// for each Unit
	for (auto & rangedUnit : rangedUnits)
	{
		// Don't stand in a storm etc
		bool fleeYouFools = false;
		for (const auto & effect : effects)
		{
			if (Util::isBadEffect(effect.effect_id, rangedUnit->isFlying()))
			{
				const float radius = m_bot.Observation()->GetEffectData()[effect.effect_id].radius;
				for (const auto & pos : effect.positions)
				{
					Drawing::drawSphere(m_bot, pos, radius, sc2::Colors::Purple);
					const float dist = Util::DistSq(rangedUnit->getPos(), pos);
					if (dist < std::powf(radius + 2.0f, 2))
					{
						sc2::Point2D fleeingPos;
						if (effect.positions.size() == 1)
						{
							if (dist > 0)
							{
								fleeingPos = pos + Util::normalizeVector(rangedUnit->getPos() - pos, radius + 2.0f);
							}
							else
							{
								fleeingPos = pos + sc2::Point2D(1.0f, 1.0f);
							}
						}
						else
						{
							const sc2::Point2D attackDirection = effect.positions.back() - effect.positions.front();
							// "Randomly" go right and left
							if (rangedUnit->getTag() % 2)
							{
								fleeingPos = rangedUnit->getPos() + Util::normalizeVector(sc2::Point2D(-attackDirection.x, attackDirection.y), radius + 2.0f);
							}
							else
							{
								fleeingPos = rangedUnit->getPos() - Util::normalizeVector(sc2::Point2D(-attackDirection.x, attackDirection.y), radius + 2.0f);
							}
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
		//
		bool breach = false;
		if (order.getType() == SquadOrderTypes::Defend && m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() <= 2)
		{
			CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
			if (Bunker.size() > 0 && Bunker.front()->isCompleted())
			{
				const BaseLocation * base = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
				const int dist = base->getGroundDistance(Bunker.front()->getPos());
				for (const auto & target : rangedUnitTargets)
				{
					if (dist > base->getGroundDistance(target->getPos()))
					{
						breach = true;
						break;
					}
				}
			}
		}

		if (rangedUnit->getTag() % speedFactor == counter%speedFactor)
		{
			// continue;
		}
		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::GuardDuty)
		{
			// find the best target for this rangedUnit
			// medivacs have the other ranged units as target.
			if (rangedUnit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				// find the nearest enemy
				CUnit_ptr nearestEnemy = nullptr;
				float minDistTarget = std::numeric_limits<float>::max();
				for (const auto & target : rangedUnitTargets)
				{
					if (target->isAlive() && rangedUnit->canHitMe(target))
					{
						float distSq = Util::DistSq(rangedUnit->getPos(), target->getPos());
						if (distSq < std::powf(target->getAttackRange(), 2) && minDistTarget > distSq)
						{
							nearestEnemy = target;
							minDistTarget = distSq;
						}
					}
				}
				if (injuredUnits.size() > 0)
				{
					CUnit_ptr mostInjured = (injuredUnits.rbegin())->second;
					if (nearestEnemy && Util::DistSq(rangedUnit->getPos(), nearestEnemy->getPos()) < Util::DistSq(mostInjured->getPos(), nearestEnemy->getPos()))
					{
						Micro::SmartCDAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
						sc2::Point2D targetPos = rangedUnit->getPos();
						sc2::Point2D runningVector = Util::normalizeVector(rangedUnit->getPos() - nearestEnemy->getPos(), nearestEnemy->getAttackRange(rangedUnit) + 2);
						targetPos += runningVector;
						Micro::SmartMove(rangedUnit, targetPos, m_bot);
					}
					else if (rangedUnit->getOrders().empty() || rangedUnit->getOrders()[0].ability_id != sc2::ABILITY_ID::EFFECT_HEAL)
					{
						if (Util::DistSq(rangedUnit->getPos(), mostInjured->getPos()) > 16.0f)
						{
							Micro::SmartCDAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
							Micro::SmartMove(rangedUnit, mostInjured, m_bot);
						}
						else
						{
							Micro::SmartAbility(rangedUnit, sc2::ABILITY_ID::EFFECT_HEAL, mostInjured, m_bot);
							injuredUnits.erase(std::prev(injuredUnits.end()));  // no idea why rbegin is not working
						}
					}
				}
				else
				{
					if (frontSoldier && (rangedUnit->getOrders().empty() ||  rangedUnit->getOrders()[0].target_unit_tag && rangedUnit->getOrders()[0].target_unit_tag != frontSoldier->getTag()))
					{
						Micro::SmartMove(rangedUnit, frontSoldier, m_bot);
					}
				}
			}
			else
			{
				// Handling disrupter shots
				if (!rangedUnit->isFlying())
				{
					const CUnits disruptorShots = m_bot.UnitInfo().getUnits(Players::Enemy, sc2::UNIT_TYPEID::PROTOSS_DISRUPTORPHASED);
					bool fleeYouFoolsPart2 = false;
					for (const auto & shot : disruptorShots)
					{
						const float distSq = Util::DistSq(rangedUnit->getPos(), shot->getPos());
						if (distSq < 25.0f)
						{
							sc2::Point2D fleeingPos;
							const sc2::Point2D pos = rangedUnit->getPos();
							if (distSq > 0)
							{
								fleeingPos = pos + Util::normalizeVector(pos - shot->getPos());
							}
							else
							{
								fleeingPos = pos + sc2::Point2D(1.0f, 1.0f);
							}
							Micro::SmartMove(rangedUnit, fleeingPos, m_bot);
							fleeYouFoolsPart2 = true;
							break;
						}
					}
					if (fleeYouFoolsPart2)
					{
						continue;
					}
				}

				// Search for target
				if (!rangedUnitTargets.empty() || (order.getType() == SquadOrderTypes::Defend && Util::DistSq(rangedUnit->getPos(), order.getPosition()) > 42.0f))
				{
					// CUnit_ptr target = getTarget(rangedUnit, rangedUnitTargets);
					// Highest prio in range target, in sight target, visible target
					const auto targets = getTarget(rangedUnit, sortedUnitTargets);
					if (targets[2].second)
					{
						Drawing::drawLine(m_bot, rangedUnit->getPos(), targets[2].second->getPos(), sc2::Colors::Green);
					}
					if (targets[1].second)
					{
						Drawing::drawLine(m_bot, rangedUnit->getPos(), targets[1].second->getPos(), sc2::Colors::Yellow);
					}
					if (targets[0].second)
					{
						Drawing::drawLine(m_bot, rangedUnit->getPos(), targets[0].second->getPos(), sc2::Colors::Red);
					}
					// if something goes wrong
					if (!targets[2].second)
					{
						// This can happen with vikings
						if (frontSoldier && (rangedUnit->getOrders().empty() || rangedUnit->getOrders().front().target_unit_tag && rangedUnit->getOrders().front().target_unit_tag != frontSoldier->getTag()))
						{
							Micro::SmartAttackMoveToPos({ rangedUnit }, frontSoldier->getPos(), m_bot);
						}
						continue;
					}
					if (order.getType() == SquadOrderTypes::Defend)
					{
						CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
						if (Bunker.size() > 0 && Bunker.front()->isCompleted())
						{
							if (Bunker.front()->getCargoSpaceTaken() != Bunker.front()->getCargoSpaceMax())
							{
								if (!targets[0].second || Util::DistSq(rangedUnit->getPos(), Bunker.front()->getPos()) < Util::DistSq(rangedUnit->getPos(), targets[0].second->getPos()))
								{
									if (Util::DistSq(rangedUnit->getPos(), Bunker.front()->getPos()) < 36.0f)
									{
										Micro::SmartRightClick(rangedUnit, Bunker.front(), m_bot);
										Micro::SmartAbility(Bunker.front(), sc2::ABILITY_ID::LOAD, rangedUnit, m_bot);
									}
									else
									{
										Micro::SmartAttackMoveToPos({ rangedUnit }, Bunker.front()->getPos(), m_bot);
									}
									continue;
								}
							}
							if (m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() <= 2)
							{
								if (!breach)
								{
									if (targets[1].second && Util::DistSq(targets[1].second->getPos(), Bunker.front()->getPos()) > 30.25f)
									{
										if (Util::DistSq(targets[1].second->getPos(), rangedUnit->getPos()) <= std::powf(targets[1].second->getAttackRange(rangedUnit), 2))
										{
											if (targets[0].second && rangedUnit->getWeaponCooldown())
											{
												Micro::SmartAttackUnit(rangedUnit, targets[0].second, m_bot);
											}
											else
											{
												sc2::Point2D retreatPos = m_bot.Bases().getNewestExpansion(Players::Self);
												Micro::SmartAttackMoveToPos({ rangedUnit }, retreatPos, m_bot);
											}
											continue;
										}
										else
										{
											Micro::SmartAbility(rangedUnit, sc2::ABILITY_ID::HOLDPOSITION, m_bot);
											continue;
										}
									}
								}
							}
						}
					}
					// We only need fancy micro if we are in range and its not a building
					if (rangedUnit->isSelected())
					{
						int a = 1;
					}
					if (targets[0].second)
					{
						// if the target in range is really the best target
						if (targets[0].first == targets[1].first)
						{
							if (targets[0].second->isBuilding())
							{
								targetsAttackedBy[targets[0].second].push_back(rangedUnit);
							}
							else
							{
								Micro::SmartKiteTarget(rangedUnit, targets[0].second, m_bot);
							}
						}
						else
						{
							if (targets[0].second->isBuilding())
							{
								if (rangedUnit->getWeaponCooldown() > 0)
								{
									targetsMovedTo[targets[1].second].push_back(rangedUnit);
								}
								else
								{
									targetsAttackedBy[targets[0].second].push_back(rangedUnit);
								}
							}
							else
							{
								targetsAttackedBy[targets[0].second].push_back(rangedUnit);
							}
						}
					}
					else if (targets[1].second)
					{
						targetsAttackedBy[targets[1].second].push_back(rangedUnit);
					}
					else
					{
						targetsAttackedBy[targets[2].second].push_back(rangedUnit);
					}
				}
				// if there are no targets
				else
				{
					// if we're not near the order position
					if (Util::DistSq(rangedUnit->getPos(), order.getPosition()) > 36.0f)
					{
						// move to it
						moveToPosition.push_back(rangedUnit);
					}
				}
			}
		}
	}
	// Grouped by target attack command
	for (const auto & t : targetsAttackedBy)
	{
		Micro::SmartAttackMoveToUnit(t.second, t.first, m_bot);
	}
	for (const auto & t : targetsMovedTo)
	{
		if (t.first->isVisible())
		{
			Micro::SmartAttackMoveToUnit(t.second, t.first, m_bot);
		}
		else
		{
			Micro::SmartAttackMoveToPos(t.second, t.first->getPos(), m_bot);
		}
	}
	// Grouped by  position Move command
	if (moveToPosition.size() > 0)
	{
		Micro::SmartAttackMoveToPos(moveToPosition, order.getPosition(), m_bot);
	}
}


// get the attack priority of a type in relation to a zergling
int RangedManager::getAttackPriority(const CUnit_ptr & unit)
{
	BOT_ASSERT(unit, "null unit in getAttackPriority");
	if (unit->isTownHall())
	{
		return 3;
	}
	if (!unit->isVisible())
	{
		return 1;
	}
	if (unit->isCombatUnit())
	{
		if (unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_BANELING)
		{
			return 9;
		}
		if (unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED || unit->isType(sc2::UNIT_TYPEID::PROTOSS_SENTRY) || unit->isType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) || unit->isType(sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED))
		{
			return 8;
		}
		if (unit->isType(sc2::UNIT_TYPEID::ZERG_MUTALISK))
		{
			return 7;
		}
		if (unit->isType(sc2::UNIT_TYPEID::ZERG_INFESTEDTERRANSEGG) || unit->isType(sc2::UNIT_TYPEID::ZERG_BROODLING) || unit->isType(sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN) || unit->isType(sc2::UNIT_TYPEID::PROTOSS_INTERCEPTOR))
		{
			return 5;
		}
		return 6;
	}
	if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS || unit->getUnitType() == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON || unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER || unit->isType(sc2::UNIT_TYPEID::TERRAN_BUNKER))
	{
		return 6;
	}
	if (unit->isWorker())
	{
		return 5;
	}
	if (unit->getUnitType() == sc2::UNIT_TYPEID::PROTOSS_PYLON || unit->getUnitType() == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER || unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
	{
		return 4;
	}
	return 1;
}

std::map<float, CUnits, std::greater<float>> RangedManager::getAttackPriority(const CUnits & enemies)
{
	std::map<float, CUnits, std::greater<float>> sortedEnemies;
	const CUnits & rangedUnits = getUnits();
	const float numUnits = static_cast<float>(rangedUnits.size());
	for (const auto & enemy : enemies)
	{
		uint32_t OpportunityLevel = 0;
		for (const auto & unit : rangedUnits)
		{
			if (enemy->canHitMe(unit))
			{
				const float distSq = Util::DistSq(unit->getPos(), enemy->getPos());
				if (distSq <= std::powf(unit->getAttackRange(), 2))
				{
					// One point for being in range
					++OpportunityLevel;
					if (unit->getEngagedTargetTag() == enemy->getTag())
					{
						// Second point for being already a target
						++OpportunityLevel;
					}
				}
			}
		}
		// 						basic priority	+	alpha*how many are already attacking it	+	beta*health
		const float priority = getAttackPriority(enemy) + 0.5f*(static_cast<float>(OpportunityLevel) / (2.0f*numUnits)) + 0.5f*(1-enemy->getHealth()/enemy->getHealthMax());
		sortedEnemies[priority].push_back(enemy);
	}
	return sortedEnemies;
}

std::vector<std::pair<float, CUnit_ptr>> RangedManager::getTarget(const CUnit_ptr & unit, const std::map<float, CUnits, std::greater<float>> & sortedEnemies)
{
	std::vector<std::pair<float, CUnit_ptr>> targets = { {-1.0f, nullptr}, { -1.0f, nullptr }, { -1.0f, nullptr } };
	const float unitAttackRange = unit->getAttackRange();
	const float unitSightRange = unit->getSightRange();

	for (const auto & enemies : sortedEnemies)
	{
		const float priority = enemies.first;
		for (const auto & enemy : enemies.second)
		{
			if (enemy->canHitMe(unit))
			{
				const float distSq = Util::DistSq(enemy->getPos(), unit->getPos());

				if (targets[2].first <= priority)
				{
					if (targets[2].second)
					{
						const float distSqOld = Util::DistSq(targets[2].second->getPos(), unit->getPos());
						const float bonusOld = unit->hasBonusDmgAgainst(targets[0].second);
						if (distSqOld > distSq || unit->hasBonusDmgAgainst(enemy) < bonusOld)
						{
							targets[2].second = enemy;
						}
					}
					else
					{
						targets[2] = { enemies.first, enemy };
					}
				}
				if (targets[1].first <= priority && distSq < std::pow(unitSightRange, 2))  // < because sometime just on the edge they are still invisible
				{
					if (targets[1].second)
					{
						const float distSqOld = Util::DistSq(targets[1].second->getPos(), unit->getPos());
						const float bonusOld = unit->hasBonusDmgAgainst(targets[0].second);
						if (distSqOld > distSq || unit->hasBonusDmgAgainst(enemy) < bonusOld)
						{
							targets[1].second = enemy;
						}
					}
					else
					{
						targets[1] = { enemies.first, enemy };
					}
				}
				if (targets[0].first <= priority && distSq <= std::pow(unitAttackRange, 2))
				{
					if (targets[0].second)
					{
						const float distSqOld = Util::DistSq(targets[0].second->getPos(), unit->getPos());
						const float bonusOld = unit->hasBonusDmgAgainst(targets[0].second);
						if (distSqOld > distSq || unit->hasBonusDmgAgainst(enemy) < bonusOld)
						{
							targets[0].second = enemy;
						}
					}
					else
					{
						targets[0] = { enemies.first, enemy };
					}
				}
			}
		}
		if (targets[0].second)
		{
			break;
		}
	}
	return targets;
}
