#include "Squad.h"
#include "CCBot.h"
#include "Util.h"

Squad::Squad(CCBot & bot)
    : m_bot(bot)
    , m_lastRetreatSwitch(0)
    , m_lastRetreatSwitchVal(false)
    , m_priority(0)
    , m_name("Default")
    , m_meleeManager(bot)
    , m_rangedManager(bot)
	, m_siegeManager(bot)
{

}

Squad::Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot)
    : m_bot(bot)
    , m_name(name)
    , m_order(order)
    , m_lastRetreatSwitch(0)
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

    // determine whether or not we should regroup
    bool needToRegroup = needsToRegroup();
    
    // if we do need to regroup, do it
    if (needToRegroup)
    {
		m_lastRetreatSwitchVal = true;
        sc2::Point2D regroupPosition = calcRegroupPosition();

        Drawing::drawSphere(m_bot,regroupPosition, 3, sc2::Colors::Purple);

        m_meleeManager.regroup(regroupPosition);
        m_rangedManager.regroup(regroupPosition);
		m_siegeManager.regroup(regroupPosition);
    }
    else // otherwise, execute micro
    {
        m_meleeManager.execute(m_order);
        m_rangedManager.execute(m_order);
		m_siegeManager.execute(m_order);
        //_detectorManager.setUnitClosestToEnemy(unitClosestToEnemy());
        //_detectorManager.execute(_order);
    }
	if (!useDebug)
	{
		return;
	}
	for (const auto & unit : m_units)
	{
		m_bot.Debug()->DebugSphereOut(unit->pos, 0.5f);
		m_bot.Debug()->DebugTextOut(m_order.getStatus(), unit->pos);
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

void Squad::setPriority(const size_t & priority)
{
    m_priority = priority;
}

void Squad::updateUnits()
{
    setAllUnits();
    setNearEnemyUnits();
    addUnitsToMicroManagers();
}

void Squad::setAllUnits()
{
    // clean up the _units vector just in case one of them died
    std::set<const sc2::Unit *> goodUnits;
    for (const auto & unit : m_units)
    {
        if (!unit) { continue; }
        if (unit->build_progress < 1.0f) { continue; }
        if (unit->health <= 0) { continue; }
		if (unit->last_seen_game_loop != m_bot.Observation()->GetGameLoop()) { continue; }
        if (m_order.getType() == SquadOrderTypes::Attack  && Util::IsWorker(unit)) { continue; }
		if (unit->cargo_space_taken>0) { continue; } //This really should not happen. No idea, why the harass medivac gets here
        goodUnits.insert(unit);
    }

    m_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
    m_nearEnemy.clear();
    for (const auto unit : m_units)
    {
        m_nearEnemy[unit] = isUnitNearEnemy(unit);

        sc2::Color color = m_nearEnemy[unit] ? m_bot.Config().ColorUnitNearEnemy : m_bot.Config().ColorUnitNotNearEnemy;
        //m_bot.Map().drawSphereAroundUnit(unitTag, color);
    }
}

void Squad::addUnitsToMicroManagers()
{
    std::vector<const sc2::Unit *> meleeUnits;
    std::vector<const sc2::Unit *> rangedUnits;
    std::vector<const sc2::Unit *> detectorUnits;
    std::vector<const sc2::Unit *> transportUnits;
    std::vector<const sc2::Unit *> siegeUnits;

    // add _units to micro managers
    for (const auto unit : m_units)
    {
        BOT_ASSERT(unit, "null unit in addUnitsToMicroManagers()");
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED)
        {
			siegeUnits.push_back(unit);
        }
        // TODO: detectors
        else if (Util::IsDetector(unit) && !m_bot.Data(unit->unit_type).isBuilding)
        {
            detectorUnits.push_back(unit);
        }
        // select ranged _units
        else if (Util::GetAttackRange(unit->unit_type, m_bot) >= 1.5f)
        {
            rangedUnits.push_back(unit);
        }
        // select melee _units
        else if (Util::GetAttackRange(unit->unit_type, m_bot) < 1.5f)
        {
            meleeUnits.push_back(unit);
        }
    }

    m_meleeManager.setUnits(meleeUnits);
    m_rangedManager.setUnits(rangedUnits);
    //m_detectorManager.setUnits(detectorUnits);
    m_siegeManager.setUnits(siegeUnits);
}

const bool Squad::needsToRegroup()
{
	if (m_order.getType() != SquadOrderTypes::Attack || m_units.size()==0 || m_bot.Observation()->GetFoodUsed()>=180)
	{
		return false;
	}
	if (m_units.size() < 100 && m_units.size()<m_bot.UnitInfo().getNumCombatUnits(Players::Enemy))
	{
		m_lastRetreatSwitchVal = true;
		return m_lastRetreatSwitchVal;
	}
	float n = 0.0f;
	sc2::Point2D mean(0.0f, 0.0f);
	sc2::Point2D variance(0.0f, 0.0f);
	for (const auto & unit : m_units)
	{
		if (!unit->is_alive || unit->unit_type.ToType()==sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			continue;
		}
		++n;
		sc2::Point2D delta = unit->pos - mean;
		mean += delta / n;
		sc2::Point2D delta2 = unit->pos - mean;
		variance += sc2::Point2D(delta.x*delta2.x, delta.y*delta2.y);
	}
	//std::cout << "std x = " << std::sqrt(variance.x / (n - 1)) << ", std y = " << std::sqrt(variance.y / (n - 1)) << std::endl;
	//std::cout << "std = " << std::sqrt((variance.x/m_bot.Map().width() + variance.y / m_bot.Map().height()) / (n - 1)) << std::endl;
	//Lets see if this is good. Actually, you should look this up. 
	float scattering = std::sqrt((variance.x / m_bot.Map().width() + variance.y / m_bot.Map().height()) / (n - 1));
	//if we are retreating, we want to do it for a while
	if (m_lastRetreatSwitchVal)
	{
		if (scattering < 1)
		{
			m_lastRetreatSwitchVal = false;
		}
	}
	else
	{
		if (scattering > 4)
		{
			m_lastRetreatSwitchVal = true;
		}
	}
	return m_lastRetreatSwitchVal;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
    m_order = so;
}

bool Squad::containsUnit(const sc2::Unit * unit) const
{
    return std::find(m_units.begin(), m_units.end(), unit) != m_units.end();
}

void Squad::clear()
{
    for (const auto & unit : getUnits())
    {
        BOT_ASSERT(unit, "null unit in squad clear");

        if (Util::IsWorker(unit))
        {
            m_bot.Workers().finishedWithWorker(unit);
        }
    }

    m_units.clear();
}

bool Squad::isUnitNearEnemy(const sc2::Unit * unit) const
{
    BOT_ASSERT(unit, "null unit in squad");

    for (const auto & u : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Enemy))
    {
        if (Util::Dist(unit->pos, u->pos) < Util::GetUnitTypeSight(u->unit_type.ToType(),m_bot))
        {
            return true;
        }
    }

    return false;
}

sc2::Point2D Squad::calcCenter() const
{
    if (m_units.empty())
    {
        return sc2::Point2D(0.0f,0.0f);
    }

    sc2::Point2D sum(0,0);
    for (const auto & unit: m_units)
    {
        BOT_ASSERT(unit, "null unit in squad calcCenter");
        sum += unit->pos;
    }

    return sc2::Point2D(sum.x / m_units.size(), sum.y / m_units.size());
}

sc2::Point2D Squad::calcRegroupPosition() const
{
    sc2::Point2D regroup= m_bot.Bases().getRallyPoint();

    if (regroup.x == 0.0f && regroup.y == 0.0f)
    {
        return m_bot.GetStartLocation();
    }
    else
    {
        return regroup;
    }
}

const sc2::Unit * Squad::unitClosestToEnemy() const
{
    const sc2::Unit * closest = nullptr;
    float closestDist = std::numeric_limits<float>::max();

    for (const auto & unit : m_units)
    {
        BOT_ASSERT(unit, "null unit");

        // the distance to the order position
        int dist = m_bot.Map().getGroundDistance(unit->pos, m_order.getPosition());

        if (dist != -1 && (!closest || dist < closestDist))
        {
            closest = unit;
            closestDist = (float)dist;
        }
    }

    return closest;
}

int Squad::squadUnitsNear(const sc2::Point2D & p) const
{
    int numUnits = 0;

    for (const auto & unit : m_units)
    {
        BOT_ASSERT(unit, "null unit");

        if (Util::Dist(unit->pos, p) < 20.0f)
        {
            numUnits++;
        }
    }

    return numUnits;
}

const std::set<const sc2::Unit *> & Squad::getUnits() const
{
    return m_units;
}

const SquadOrder & Squad::getSquadOrder()	const
{
    return m_order;
}

void Squad::addUnit(const sc2::Unit * unit)
{
    m_units.insert(unit);
}

void Squad::removeUnit(const sc2::Unit * unit)
{
    m_units.erase(unit);
}

const std::string & Squad::getName() const
{
    return m_name;
}