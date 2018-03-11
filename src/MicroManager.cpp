#include "MicroManager.h"
#include "CCBot.h"
#include "Util.h"

MicroManager::MicroManager(CCBot & bot)
	: m_bot(bot)
{
}

void MicroManager::setUnits(const std::vector<const sc2::Unit *> & u)
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
		sc2::Point2D pos(m_bot.Bases().getRallyPoint());
		for (const auto & unit : m_units)
		{
			if (Util::Dist(unit->pos, pos) > 5)
			{
				if (Util::IsCombatUnitType(unit->unit_type, m_bot))
				{
					Micro::SmartAttackMove(unit, pos, m_bot);
					const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BUNKER }));
					if (Bunker.size() > 0)
					{
						for (auto & b : Bunker)
						{
							if (b->cargo_space_taken != b->cargo_space_max)
							{
								Micro::SmartRightClick(unit, b, m_bot);
								m_bot.Actions()->UnitCommand(b, sc2::ABILITY_ID::LOAD, unit);
								break;
							}
						}
					}
				}
				else
				{
					Micro::SmartMove(unit, pos, m_bot);
				}
			}
		}
	}

	if (!(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty))
	{
		return;
	}

	order = inputOrder;

	// Discover enemies within region of interest
	std::set<const sc2::Unit *> nearbyEnemies;

	// if the order is to defend, we only care about units in the radius of the defense
	if (order.getType() == SquadOrderTypes::Defend || inputOrder.getType() == SquadOrderTypes::GuardDuty)
	{
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (Util::Dist(enemyUnit->pos, order.getPosition()) < order.getRadius())
			{
				nearbyEnemies.insert(enemyUnit);
			}
		}

	} // otherwise we want to see everything on the way as well
	else if (order.getType() == SquadOrderTypes::Attack)
	{
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (Util::Dist(enemyUnit->pos, order.getPosition()) < order.getRadius())
			{
				nearbyEnemies.insert(enemyUnit);
			}
		}

		for (const auto & unit : m_units)
		{
			BOT_ASSERT(unit, "null unit in attack");

			for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
			{
				if (Util::Dist(enemyUnit->pos, unit->pos) < order.getRadius())
				{
					nearbyEnemies.insert(enemyUnit);
				}
			}
		}
	}

	std::vector<const sc2::Unit *> targetUnitTags;
	std::copy(nearbyEnemies.begin(), nearbyEnemies.end(), std::back_inserter(targetUnitTags));

	// the following block of code attacks all units on the way to the order position
	// we want to do this if the order is attack, defend, or harass
	if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::GuardDuty)
	{
		// if this is a defense squad then we care about all units in the area
		if (order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::GuardDuty)
		{
			executeMicro(targetUnitTags);
		}
		// otherwise we only care about workers if they are in their own region
		// this is because their scout can run around and drag our units around the map
		else
		{
			// if this is the an attack squad
			std::vector<const sc2::Unit *> workersRemoved;
			for (const auto enemyUnit : targetUnitTags)
			{
				BOT_ASSERT(enemyUnit, "null enemy unit target");

				// if its not a worker add it to the targets
				if (!Util::IsWorker(enemyUnit))
				{
					workersRemoved.push_back(enemyUnit);
				}
				// if it is a worker
				else
				{
					for (const BaseLocation * enemyBaseLocation : m_bot.Bases().getOccupiedBaseLocations(Players::Enemy))
					{
						// only add it if it's in their region
						if (enemyBaseLocation->containsPosition(enemyUnit->pos))
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

const std::vector<const sc2::Unit *> & MicroManager::getUnits() const
{
	return m_units;
}

void MicroManager::regroup(const sc2::Point2D & regroupPosition) const
{
	sc2::Units regroupUnits;
	for (const auto & unit : m_units)
	{
		if (Util::Dist(unit->pos, regroupPosition) > 8.0f)
		{
			regroupUnits.push_back(unit);
		}
	}
	
	Micro::SmartAttackMove(regroupUnits, regroupPosition, m_bot);
}

void MicroManager::trainSubUnits(const sc2::Unit * unit) const
{
	// TODO: something here
}