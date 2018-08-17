#include "Squad.h"
#include "CCBot.h"
#include "Util.h"

Squad::Squad(CCBot & bot)
	: m_bot(bot)
	, m_name("Default")
	, m_status(Status::fine)
	, m_lastRetreatSwitchVal(false)
	, m_priority(0)
	, m_meleeManager(bot)
	, m_rangedManager(bot)
	, m_siegeManager(bot)
{
}

Squad::Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot)
	: m_bot(bot)
	, m_name(name)
	, m_order(order)
	, m_status(Status::fine)
	, m_lastRetreatSwitchVal(false)
	, m_priority(priority)
	, m_meleeManager(bot)
	, m_rangedManager(bot)
	, m_siegeManager(bot)
{
}

void Squad::onFrame()
{
	// update all necessary unit information within this squad
	updateUnits();
	if (m_units.size() == 0)
	{
		return;
	}
	// determine whether or not we should regroup
	bool needToRegroup = needsToRegroup();

	// if we do need to regroup, do it
	if (needToRegroup)
	{
		m_lastRetreatSwitchVal = true;
		sc2::Point2D regroupPosition = calcRegroupPosition();

		Drawing::drawSphere(m_bot, regroupPosition, 3, sc2::Colors::Purple);

		m_meleeManager.regroup(regroupPosition, m_status == Status::flee);
		m_rangedManager.regroup(regroupPosition, m_status == Status::flee);
		m_siegeManager.regroup(regroupPosition, m_status == Status::flee);
	}
	else  // otherwise, execute micro
	{
		m_meleeManager.execute(m_order);
		m_rangedManager.execute(m_order);
		m_siegeManager.execute(m_order);
	}
	for (const auto & unit : m_units)
	{
		Drawing::drawText(m_bot, unit->getPos() - sc2::Point3D(), "\n" + m_order.getStatus());
	}
}

bool Squad::isEmpty() const
{
	return m_units.empty();
}

size_t Squad::getPriority() const
{
	return m_priority;
}

/*
void Squad::setPriority(const size_t & priority)
{
	m_priority = priority;
}
*/

void Squad::updateUnits()
{
	setAllUnits();
	addUnitsToMicroManagers();
}

void Squad::setAllUnits()
{
	// clean up the _units vector just in case one of them died
	CUnits goodUnits;
	CUnits cannonRush;
	if (m_order.getType() == SquadOrderTypes::Defend && m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
	{
		const CUnits cannons = m_bot.UnitInfo().getUnits(Players::Enemy, sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON);
		for (const auto & cannon : cannons)
		{
			if (Util::DistSq(cannon->getPos(), m_order.getPosition()) < std::pow(m_order.getRadius(), 2))
			{
				cannonRush.push_back(cannon);
			}
		}
	}
	for (const auto & unit : m_units)
	{
		if (!unit) { continue; }
		if (unit->getBuildProgress() < 1.0f) { continue; }
		if (unit->getHealth() <= 0) { continue; }
		if (unit->getLastSeenGameLoop() != m_bot.Observation()->GetGameLoop()) { continue; }
		if (m_order.getType() == SquadOrderTypes::Attack  && unit->isWorker()) { continue; }
		if (unit->getOccupation().first == CUnit::Occupation::Harass) { continue; }
		if (unit->getOccupation().first == CUnit::Occupation::Shuttle) { continue; }
		if (unit->getOccupation().first == CUnit::Occupation::Scout) { continue; }
		if (unit->getOccupation().first == CUnit::Occupation::Worker && unit->getOccupation().second != WorkerJobs::Combat) { continue; }
		if (unit->changedUnitType()) { continue; }
		if (m_order.getType() == SquadOrderTypes::Defend && m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss && cannonRush.size() > 4 * m_units.size()) { continue; }
		goodUnits.push_back(unit);
	}

	m_units = goodUnits;
}

/*
void Squad::setNearEnemyUnits()
{
	m_nearEnemy.clear();
	for (const auto unit : m_units)
	{
		m_nearEnemy[unit] = isUnitNearEnemy(unit);

		sc2::Color color = m_nearEnemy[unit] ? m_bot.Config().ColorUnitNearEnemy : m_bot.Config().ColorUnitNotNearEnemy;
	}
}
*/

void Squad::addUnitsToMicroManagers()
{
	CUnits meleeUnits;
	CUnits rangedUnits;
	CUnits detectorUnits;
	// CUnits transportUnits;
	CUnits siegeUnits;

	// add _units to micro managers
	for (const auto unit : m_units)
	{
		BOT_ASSERT(unit, "null unit in addUnitsToMicroManagers()");
		if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANK || unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
		{
			siegeUnits.push_back(unit);
		}
		// TODO: detectors
		else if (unit->isDetector() && !unit->isBuilding())
		{
			detectorUnits.push_back(unit);
		}
		// select ranged _units
		else if (unit->getAttackRange()>= 1.5f)
		{
			rangedUnits.push_back(unit);
		}
		// select melee _units
		else if (unit->getAttackRange() < 1.5f)
		{
			meleeUnits.push_back(unit);
		}
	}

	m_meleeManager.setUnits(meleeUnits);
	m_rangedManager.setUnits(rangedUnits);
	// m_detectorManager.setUnits(detectorUnits);
	m_siegeManager.setUnits(siegeUnits);
}

const bool Squad::needsToRegroup()
{
	if (m_order.getType() == SquadOrderTypes::Attack)
	{
		//std::cout << m_bot.UnitInfo().getFoodCombatUnits(Players::Self) << " < " << m_bot.UnitInfo().getFoodCombatUnits(Players::Enemy) << std::endl;
		if ((m_bot.Observation()->GetFoodArmy() < 100 && m_bot.UnitInfo().getFoodCombatUnits(Players::Self) < m_bot.UnitInfo().getFoodCombatUnits(Players::Enemy)) || (m_bot.Observation()->GetFoodUsed() < 195 && m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) < m_bot.UnitInfo().getUnitTypeCount(Players::Enemy, sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)))
		{
			m_bot.retreat();
			m_status = Status::flee;
			m_lastRetreatSwitchVal = true;
			return m_lastRetreatSwitchVal;
		}
		std::map<CUnit_ptr, size_t> inSight;
		std::map<CUnit_ptr, size_t> inSightEnemy;
		for (size_t i = 0; i < m_units.size(); ++i)
		{
			if (!m_units[i]->isAlive() || m_units[i]->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				continue;
			}
			for (size_t j = i; j < m_units.size(); ++j)
			{
				if (!m_units[j]->isAlive() || m_units[j]->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
				{
					continue;
				}
				if (Util::DistSq(m_units[i]->getPos(), m_units[j]->getPos()) < 225.0f)
				{
					++inSight[m_units[i]];
					++inSight[m_units[j]];
				}
			}
			for (const auto & enemy : m_bot.UnitInfo().getUnits(Players::Enemy))
			{
				if (enemy->isCombatUnit() && Util::DistSq(m_units[i]->getPos(), enemy->getPos()) < std::pow(m_units[i]->getSightRange(), 2))
				{
					inSightEnemy[m_units[i]]++;
				}
			}
		}
		size_t maxCount = 0;
		for (const auto & sightCount : inSight)
		{
			maxCount = std::max<size_t>(maxCount, sightCount.second);
		}
		const float scattering = static_cast<float>(maxCount)/ static_cast<float>(std::max<size_t>(1, inSight.size()));

		size_t maxCountEnemy = 0;
		for (const auto & sightCount : inSightEnemy)
		{
			maxCountEnemy = std::max<size_t>(maxCountEnemy, sightCount.second);
		}

		// if we are retreating, we want to do it for a while
		if (m_status == Status::fine)
		{
			if (maxCountEnemy > std::min<size_t>(2, maxCount) && scattering < 0.55f)
			{
				m_status = Status::regroup;
				m_lastRetreatSwitchVal = true;
			}
		}
		else
		{
			if (scattering > 0.8f)
			{
				m_status = Status::fine;
				m_lastRetreatSwitchVal = false;
			}
		}
		/* Does not work good
		float n = 0.0f;
		sc2::Point2D mean(0.0f, 0.0f);
		sc2::Point2D variance(0.0f, 0.0f);
		for (const auto & unit : m_units)
		{
			if (!unit->isAlive() || unit->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				continue;
			}
			++n;
			sc2::Point2D delta = unit->getPos() - mean;
			mean += delta / n;
			sc2::Point2D delta2 = unit->getPos() - mean;
			variance += sc2::Point2D(delta.x*delta2.x, delta.y*delta2.y);
		}
		// std::cout << "std x = " << std::sqrt(variance.x / (n - 1)) << ", std y = " << std::sqrt(variance.y / (n - 1)) << std::endl;
		// std::cout << "std = " << std::sqrt((variance.x/m_bot.Map().width() + variance.y / m_bot.Map().height()) / (n - 1)) << std::endl;
		// Lets see if this is good. Actually, you should look this up. 
		const float scattering = std::sqrt((variance.x / m_bot.Map().width() + variance.y / m_bot.Map().height()) / (n - 1));
		// if we are retreating, we want to do it for a while
		if (m_lastRetreatSwitchVal)
		{
			if (scattering < 1.0f)
			{
				m_lastRetreatSwitchVal = false;
			}
		}
		else
		{
			if (scattering > 4.0f)
			{
				m_lastRetreatSwitchVal = true;
			}
		}
		*/
	}
	else if (m_order.getType() == SquadOrderTypes::Defend)
	{
		const sc2::Point2D startingBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase();
		const sc2::Point2D newestBase = m_bot.Bases().getNewestExpansion(Players::Self);
		if ( m_bot.UnitInfo().getUnitTypeCount(Players::Self, Util::getTownHallTypes()) == 1 || (m_order.getPosition().x == startingBase.x && m_order.getPosition().y == startingBase.y) || !(m_order.getPosition().x == newestBase.x && m_order.getPosition().y == newestBase.y) || m_bot.Observation()->GetFoodUsed() >= 180)
		{
			return false;
		}
		CUnits nearbyEnemies;
		const sc2::Point2D regroup = calcRegroupPosition();
		for (const auto & enemyUnit : m_bot.UnitInfo().getUnits(Players::Enemy))
		{
			if (enemyUnit->isVisible() && Util::DistSq(enemyUnit->getPos(), m_order.getPosition()) < std::powf(m_order.getRadius(), 2))
			{
				nearbyEnemies.push_back(enemyUnit);
			}
			if (Util::DistSq(enemyUnit->getPos(), regroup) < 100.0f)
			{
				return false;
			}
		}
		return nearbyEnemies.size() > m_units.size();
	}
	else
	{
		return false;
	}
	return m_lastRetreatSwitchVal;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	m_order = so;
}

bool Squad::containsUnit(const CUnit_ptr unit) const
{
	return std::find_if(m_units.begin(), m_units.end(), [& unit](const CUnit_ptr & newUnit) {return unit->getTag() == newUnit->getTag(); }) != m_units.end();
}

void Squad::clear()
{
	for (const auto & unit : getUnits())
	{
		BOT_ASSERT(unit, "null unit in squad clear");

		if (unit->isWorker())
		{
			m_bot.Workers().finishedWithWorker(unit);
		}
		else
		{
			unit->setOccupation({ unit->getOccupation().first, 0 });
		}
	}

	m_units.clear();
}

bool Squad::isUnitNearEnemy(CUnit_ptr unit) const
{
	BOT_ASSERT(unit, "null unit in squad");

	for (const auto & u : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		if (Util::Dist(unit->getPos(), u->getPos()) < u->getSightRange())
		{
			return true;
		}
	}

	return false;
}

/*
sc2::Point2D Squad::calcCenter() const
{
	if (m_units.empty())
	{
		return sc2::Point2D(0.0f, 0.0f);
	}

	sc2::Point2D sum(0.0f, 0.0f);
	for (const auto & unit : m_units)
	{
		BOT_ASSERT(unit, "null unit in squad calcCenter");
		sum += unit->getPos();
	}

	return sc2::Point2D(sum.x / m_units.size(), sum.y / m_units.size());
}
*/

sc2::Point2D Squad::calcRegroupPosition() const
{
	if (m_status == Status::flee)
	{
		sc2::Point2D regroup = m_bot.Bases().getRallyPoint();

		if (regroup.x == 0.0f && regroup.y == 0.0f)
		{
			return m_bot.GetStartLocation();
		}
		else
		{
			return regroup;
		}
	}
	else
	{
		return Util::CalcCenter(m_units);
	}
}

/*
const CUnit_ptr Squad::unitClosestToEnemy() const
{
	CUnit_ptr closest = nullptr;
	float closestDist = std::numeric_limits<float>::max();

	for (const auto & unit : m_units)
	{
		BOT_ASSERT(unit, "null unit");

		// the distance to the order position
		int dist = m_bot.Map().getGroundDistance(unit->getPos(), m_order.getPosition());

		if (dist != -1 && (!closest || dist < closestDist))
		{
			closest = unit;
			closestDist = (float)dist;
		}
	}

	return closest;
}
*/

/*
int Squad::squadUnitsNear(const sc2::Point2D & p) const
{
	int numUnits = 0;

	for (const auto & unit : m_units)
	{
		BOT_ASSERT(unit, "null unit");

		if (Util::Dist(unit->getPos(), p) < 20.0f)
		{
			numUnits++;
		}
	}

	return numUnits;
}
*/

const CUnits & Squad::getUnits() const
{
	return m_units;
}

const SquadOrder & Squad::getSquadOrder()	const
{
	return m_order;
}

void Squad::addUnit(const CUnit_ptr unit)
{
	m_units.push_back(unit);
	unit->setOccupation({ CUnit::Occupation::Combat, static_cast<int>(getPriority()) });
}

void Squad::removeUnit(const CUnit_ptr unit)
{
	if (unit->isWorker())
	{
		m_bot.Workers().finishedWithWorker(unit);
	}
	m_units.erase(std::remove_if(m_units.begin(), m_units.end(), [unit](CUnit_ptr & newUnit) {return unit->getTag() == newUnit->getTag(); }), m_units.end());
}

const std::string & Squad::getName() const
{
	return m_name;
}
