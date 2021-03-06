#include "MicroManager.h"
#include "CCBot.h"
#include "Util.h"

MicroManager::MicroManager(CCBot & bot)
	: m_bot(bot)
{
}

void MicroManager::setUnits(CUnits & u)
{
	m_units = u;
}

void MicroManager::execute(const SquadOrder & inputOrder)
{
	// Nothing to do if we have no units
	if (m_units.empty())
	{
		return;
	}
	if (inputOrder.getType() == SquadOrderTypes::Idle)
	{
		const sc2::Point2D pos(m_bot.Bases().getRallyPoint());
		CUnits moveUnits;
		CUnits attackMoveUnits;
		for (const auto & unit : m_units)
		{
			if (Util::DistSq(unit->getPos(), pos) > 25.0f)
			{
				if (unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC))
				{
					Micro::SmartCDAbility(unit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
					moveUnits.push_back(unit);
					continue;
				}
				else if (unit->isCombatUnit())
				{
					CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
					bool bunkerTime = false;
					if (Bunker.size() > 0)
					{
						for (const auto & b : Bunker)
						{
							if (b->getCargoSpaceTaken() != b->getCargoSpaceMax() && b->isCompleted())
							{
								if (Util::DistSq(b->getPos(), unit->getPos()) < 36.0f)
								{
									Micro::SmartRightClick(unit, b, m_bot);
									Micro::SmartAbility(b, sc2::ABILITY_ID::LOAD, unit, m_bot);
								}
								else
								{
									attackMoveUnits.push_back(unit);
								}
								bunkerTime = true;
								break;
							}
						}
					}
					if (!bunkerTime)
					{
						attackMoveUnits.push_back(unit);
					}
				}
				else
				{
					moveUnits.push_back(unit);
				}
			}
		}
		if (moveUnits.size() > 0)
		{
			Micro::SmartMove(moveUnits, pos, m_bot);
		}
		if (attackMoveUnits.size() > 0)
		{
			Micro::SmartAttackMoveToPos(attackMoveUnits, pos, m_bot);
		}
		return;
	}

	if (!(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty))
	{
		return;
	}

	order = inputOrder;

	// Discover enemies within region of interest
	CUnits nearbyEnemies;

	if (order.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty || order.getType() == SquadOrderTypes::Attack)
	{
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (Util::DistSq(enemyUnit->getPos(), order.getPosition()) < std::powf(order.getRadius(), 2))
			{
				nearbyEnemies.push_back(enemyUnit);
			}
			else if (order.getType() == SquadOrderTypes::Attack)
			{
				for (const auto & unit : m_units)
				{
					if (Util::DistSq(enemyUnit->getPos(), unit->getPos()) < std::pow(order.getRadius(), 2))
					{
						nearbyEnemies.push_back(enemyUnit);
					}
				}
			}
		}
	}

	// the following block of code attacks all units on the way to the order position
	// we want to do this if the order is attack, defend, or harass
	if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::GuardDuty)
	{
		// if this is a defense squad then we care about all units in the area
		if (order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::GuardDuty)
		{
				executeMicro(nearbyEnemies);
		}
		// otherwise we only care about workers if they are in their own region
		// this is because their scout can run around and drag our units around the map
		else
		{
			// if this is the an attack squad
			CUnits workersRemoved;
			for (const auto enemyUnit : nearbyEnemies)
			{
				BOT_ASSERT(enemyUnit, "null enemy unit target");

				// if its not a worker add it to the targets
				if (!enemyUnit->isWorker())
				{
					workersRemoved.push_back(enemyUnit);
				}
				// if it is a worker
				else
				{
					for (const BaseLocation * enemyBaseLocation : m_bot.Bases().getOccupiedBaseLocations(Players::Enemy))
					{
						// only add it if it's in their region
						if (enemyBaseLocation->containsPosition(enemyUnit->getPos()))
						{
							workersRemoved.push_back(enemyUnit);
						}
					}
				}
			}

			// Allow micromanager to handle enemies
			executeMicro(workersRemoved);
		}
	}
}

const CUnits & MicroManager::getUnits() const
{
	return m_units;
}

void MicroManager::regroup(const sc2::Point2D & regroupPosition, const bool fleeing) const
{
	CUnits regroupUnitsMove;
	CUnits regroupUnitsShoot;
	CUnits regroupUnitsAttackMove;
	for (const auto & unit : m_units)
	{
		if (unit->isSelected())
		{
			int a = 1;
		}
		if (Util::DistSq(unit->getPos(), regroupPosition) > 56.0f)
		{
			if (unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC))
			{
				Micro::SmartCDAbility(unit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
				regroupUnitsMove.push_back(unit);
			}
			else if (unit->isType(sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED))
			{
				Micro::SmartAbility(unit, sc2::ABILITY_ID::MORPH_UNSIEGE, m_bot);
			}
			else if (unit->isType(sc2::UNIT_TYPEID::TERRAN_SCV))
			{
				m_bot.Workers().finishedWithWorker(unit);
			}
			else
			{
				regroupUnitsShoot.push_back(unit);
			}
		}
		else
		{
			if (unit->isType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) && fleeing)
			{
				Micro::SmartAbility(unit, sc2::ABILITY_ID::MORPH_SIEGEMODE, m_bot);
			}
			else if (unit->isCombatUnit() && !unit->getOrders().empty() && unit->getOrders().front().ability_id == sc2::ABILITY_ID::MOVE)
			{
				regroupUnitsAttackMove.push_back(unit);
			}
		}
	}
	if (!regroupUnitsMove.empty())
	{
		Micro::SmartMove(regroupUnitsMove, regroupPosition, m_bot);
	}
	if (!regroupUnitsShoot.empty())
	{
		Micro::SmartAttackMoveToPos(regroupUnitsShoot, regroupPosition, m_bot);
	}
	if (!regroupUnitsAttackMove.empty())
	{
		Micro::SmartAttackMove(regroupUnitsAttackMove, regroupPosition, m_bot);
	}
}

/*
void MicroManager::trainSubUnits(CUnit_ptr unit) const
{
	// something here
}
*/