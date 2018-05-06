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
			if (Util::Dist(unit->getPos(), pos) > 5.0f)
			{
				if (unit->isCombatUnit())
				{
					CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
					bool bunkerTime = false;
					if (Bunker.size() > 0)
					{
						for (const auto & b : Bunker)
						{
							if (b->getCargoSpaceTaken() != b->getCargoSpaceMax())
							{
								Micro::SmartRightClick(unit, b, m_bot);
								Micro::SmartAbility(b, sc2::ABILITY_ID::LOAD, unit,m_bot);
								bunkerTime = true;
								break;
							}
						}
					}
					if (!bunkerTime)
					{
						attackMoveUnits.push_back(unit);
					}
					if (unit->isType(sc2::UNIT_TYPEID::TERRAN_MEDIVAC))
					{
						Micro::SmartCDAbility(unit, sc2::ABILITY_ID::EFFECT_MEDIVACIGNITEAFTERBURNERS, m_bot);
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
			Micro::SmartAttackMove(attackMoveUnits, pos, m_bot);
		}
	}

	if (!(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty))
	{
		return;
	}

	order = inputOrder;

	// Discover enemies within region of interest
	CUnits nearbyEnemies;

	// if the order is to defend, we only care about units in the radius of the defense
	if (order.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty)
	{
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (Util::Dist(enemyUnit->getPos(), order.getPosition()) < order.getRadius())
			{
				nearbyEnemies.push_back(enemyUnit);
			}
		}

	} // otherwise we want to see everything on the way as well
	else if (order.getType() == SquadOrderTypes::Attack)
	{
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (Util::Dist(enemyUnit->getPos(), order.getPosition()) < order.getRadius())
			{
				nearbyEnemies.push_back(enemyUnit);
			}
		}

		for (const auto & unit : m_units)
		{
			BOT_ASSERT(unit, "null unit in attack");

			for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
			{
				if (Util::Dist(enemyUnit->getPos(), unit->getPos()) < order.getRadius())
				{
					nearbyEnemies.push_back(enemyUnit);
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

void MicroManager::regroup(const sc2::Point2D & regroupPosition) const
{
	CUnits regroupUnitsMove;
	CUnits regroupUnitsShoot;
	for (const auto & unit : m_units)
	{
		if (Util::Dist(unit->getPos(), regroupPosition) > 8.0f)
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
			else
			{
				if (unit->getWeaponCooldown())
				{
					regroupUnitsShoot.push_back(unit);
				}
				else
				{
					regroupUnitsMove.push_back(unit);
				}
			}
		}

	}
	if (regroupUnitsMove.size() > 0)
	{
		Micro::SmartMove(regroupUnitsShoot, regroupPosition, m_bot);
	}
	if (regroupUnitsShoot.size() > 0)
	{
		Micro::SmartAttackMove(regroupUnitsShoot, regroupPosition, m_bot);
	}

	
}

void MicroManager::trainSubUnits(CUnit_ptr unit) const
{
	// TODO: something here
}