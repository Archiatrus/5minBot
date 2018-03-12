#include "SquadData.h"
#include "CCBot.h"
#include "Util.h"

SquadData::SquadData(CCBot & bot)
	: m_bot(bot)
{

}

void SquadData::onFrame()
{
	updateAllSquads();
	verifySquadUniqueMembership();
	drawSquadInformation();
}

void SquadData::clearSquadData()
{
	// give back workers who were in squads
	for (auto & kv : m_squads)
	{
		Squad & squad = kv.second;
		for (const auto & unit : squad.getUnits())
		{
			BOT_ASSERT(unit, "null unit");
			if (unit->isWorker())
			{
				m_bot.Workers().finishedWithWorker(unit);
			}
		}
	}

	m_squads.clear();
}

void SquadData::removeSquad(const std::string & squadName)
{
	const auto & squadPtr = m_squads.find(squadName);

	BOT_ASSERT(squadPtr != m_squads.end(), "Trying to clear a squad that didn't exist: %s", squadName.c_str());
	if (squadPtr == m_squads.end())
	{
		return;
	}

	for (const auto unit : squadPtr->second.getUnits())
	{
		BOT_ASSERT(unit, "null unit");

		if (unit->isWorker())
		{
			m_bot.Workers().finishedWithWorker(unit);
		}
	}

	m_squads.erase(squadName);
}

const std::map<std::string, Squad> & SquadData::getSquads() const
{
	return m_squads;
}

bool SquadData::squadExists(const std::string & squadName)
{
	return m_squads.find(squadName) != m_squads.end();
}

void SquadData::addSquad(const std::string & squadName, const Squad & squad)
{
	m_squads.insert(std::pair<std::string, Squad>(squadName, squad));
}

void SquadData::updateAllSquads()
{
	for (auto & kv : m_squads)
	{
		kv.second.onFrame();
	}
}

void SquadData::drawSquadInformation()
{
	if (!m_bot.Config().DrawSquadInfo)
	{
		return;
	}

	std::stringstream ss;
	ss << "Squad Data\n\n";

	for (auto & kv : m_squads)
	{
		const Squad & squad = kv.second;

		auto & units = squad.getUnits();
		const SquadOrder & order = squad.getSquadOrder();

		ss << squad.getName() << " " << units.size() << " (";
		ss << (int)order.getPosition().x << ", " << (int)order.getPosition().y << ")\n";

		Drawing::drawSphere(m_bot,order.getPosition(), 5, sc2::Colors::Red);
		Drawing::drawText(m_bot,order.getPosition(), squad.getName(), sc2::Colors::Red);

		for (const auto & unit : units)
		{
			BOT_ASSERT(unit, "null unit");

			Drawing::drawText(m_bot,unit->getPos(), squad.getName(), sc2::Colors::Green);
		}
	}

	Drawing::drawTextScreen(m_bot,sc2::Point2D(0.5f, 0.2f), ss.str(), sc2::Colors::Red);
}

void SquadData::verifySquadUniqueMembership()
{
	CUnits assigned;

	for (const auto & kv : m_squads)
	{
		for (const auto & unit : kv.second.getUnits())
		{
			if (std::find_if(assigned.begin(), assigned.end(), [unit](CUnit_ptr & newUnit) {return unit->getTag() == newUnit->getTag(); }) != assigned.end())
			{
				std::cout << "Warning: Unit is in at least two squads: " << unit->getTag() << "\n";
			}

			assigned.push_back(unit);
		}
	}
}

bool SquadData::unitIsInSquad(const CUnit_ptr unit) const
{
	return getUnitSquad(unit) != nullptr;
}

const Squad * SquadData::getUnitSquad(const CUnit_ptr unit) const
{
	for (const auto & kv : m_squads)
	{
		if (kv.second.containsUnit(unit))
		{
			return &kv.second;
		}
	}

	return nullptr;
}

Squad * SquadData::getUnitSquad(const CUnit_ptr unit)
{
	for (auto & kv : m_squads)
	{
		if (kv.second.containsUnit(unit))
		{
			return &kv.second;
		}
	}

	return nullptr;
}

void SquadData::assignUnitToSquad(const CUnit_ptr unit, Squad & squad)
{
	BOT_ASSERT(canAssignUnitToSquad(unit, squad), "We shouldn't be re-assigning this unit!");

	Squad * previousSquad = getUnitSquad(unit);

	if (previousSquad)
	{
		previousSquad->removeUnit(unit);
	}

	squad.addUnit(unit);
}

bool SquadData::canAssignUnitToSquad(const CUnit_ptr unit, const Squad & squad) const
{

	const Squad * unitSquad = getUnitSquad(unit);
	//It really should not happen, that the harass medivac reaches here. No idea how this happens
	if (unit->getCargoSpaceTaken() > 0)
	{
		return false;
	}
	// make sure strictly less than so we don't reassign to the same squad etc
	return !unitSquad || (unitSquad->getPriority() < squad.getPriority());
}

Squad & SquadData::getSquad(const std::string & squadName)
{
	BOT_ASSERT(squadExists(squadName), "Trying to access squad that doesn't exist: %s", squadName.c_str());
	return m_squads.at(squadName);
}


const bool SquadData::underAttack() const
{
	for (const auto & squad : m_squads)
	{
		if (squad.second.getSquadOrder().getType() == SquadOrderTypes::Defend)
		{
			return true;
		}
	}
	return false;
}