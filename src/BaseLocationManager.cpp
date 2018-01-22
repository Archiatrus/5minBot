#include "BaseLocationManager.h"
#include "Util.h"
#include "sc2lib/sc2_search.h"
#include "Drawing.h"
#include "CCBot.h"

BaseLocationManager::BaseLocationManager(CCBot & bot)
    : m_bot(bot)
{
    
}

void BaseLocationManager::onStart()
{
	//sc2::search::CalculateExpansionLocations(m_bot.Observation(),m_bot.Query())

    m_tileBaseLocations = std::vector<std::vector<BaseLocation *>>(m_bot.Map().width(), std::vector<BaseLocation *>(m_bot.Map().height(), nullptr));
    m_playerStartingBaseLocations[Players::Self]  = nullptr;
    m_playerStartingBaseLocations[Players::Enemy] = nullptr; 
    
    // a BaseLocation will be anything where there are minerals to mine
    // so we will first look over all minerals and cluster them based on some distance
    const int clusterDistance = 14;
    
    // stores each cluster of resources based on some ground distance
    std::vector<std::vector<const sc2::Unit *>> resourceClusters;
    for (const auto & mineral : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral))
    {
        // skip minerals that don't have more than 100 starting minerals
        // these are probably stupid map-blocking minerals to confuse us
        if (!Util::IsMineral(mineral))
        {
            continue;
        }

        bool foundCluster = false;
        for (auto & cluster : resourceClusters)
        {
            float dist = Util::Dist(mineral->pos, Util::CalcCenter(cluster));
            
            // quick initial air distance check to eliminate most resources
            if (dist < clusterDistance)
            {
                // now do a more expensive ground distance check
                float groundDist = dist; //m_bot.Map().getGroundDistance(mineral.pos, Util::CalcCenter(cluster));
                if (groundDist >= 0 && groundDist < clusterDistance)
                {
                    cluster.push_back(mineral);
                    foundCluster = true;
                    break;
                }
            }
        }

        if (!foundCluster)
        {
            resourceClusters.push_back(std::vector<const sc2::Unit *>());
            resourceClusters.back().push_back(mineral);
        }
    }

    // add geysers only to existing resource clusters
    for (const auto & geyser : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral))
    {
        if (!Util::IsGeyser(geyser))
        {
            continue;
        }

        for (auto & cluster : resourceClusters)
        {
            //int groundDist = m_bot.Map().getGroundDistance(geyser.pos, Util::CalcCenter(cluster));
            float groundDist = Util::Dist(geyser->pos, Util::CalcCenter(cluster));

            if (groundDist >= 0 && groundDist < clusterDistance)
            {
                cluster.push_back(geyser);
                break;
            }
        }
    }

    // add the base locations if there are more than 4 resouces in the cluster
    int baseID = 0;
    for (const auto & cluster : resourceClusters)
    {
        if (cluster.size() > 4)
        {
            m_baseLocationData.push_back(BaseLocation(m_bot, baseID++, cluster));
        }
    }

    // construct the vectors of base location pointers, this is safe since they will never change
    for (const auto & baseLocation : m_baseLocationData)
    {
        m_baseLocationPtrs.push_back(&baseLocation);

        // if it's a start location, add it to the start locations
        if (baseLocation.isStartLocation())
        {
            m_startingBaseLocations.push_back(&baseLocation);
        }

        // if it's our starting location, set the pointer
        if (baseLocation.isPlayerStartLocation(Players::Self))
        {
            m_playerStartingBaseLocations[Players::Self] = &baseLocation;
        }

        if (baseLocation.isPlayerStartLocation(Players::Enemy))
        {
            m_playerStartingBaseLocations[Players::Enemy] = &baseLocation;
        }
    }

    // construct the map of tile positions to base locations
    for (float x=0; x < m_bot.Map().width(); ++x)
    {
        for (int y=0; y < m_bot.Map().height(); ++y)
        {
            for (auto & baseLocation : m_baseLocationData)
            {
                sc2::Point2D pos(x + 0.5f, y + 0.5f);

                if (baseLocation.containsPosition(pos))
                {
                    m_tileBaseLocations[(int)x][(int)y] = &baseLocation;
                    
                    break;
                }
            }
        }
    }

    // construct the sets of occupied base locations
    m_occupiedBaseLocations[Players::Self] = std::set<const BaseLocation *>();
    m_occupiedBaseLocations[Players::Enemy] = std::set<const BaseLocation *>();

	//We know at least one of our 
}

void BaseLocationManager::onFrame()
{   
    drawBaseLocations();
    // reset the player occupation information for each location
    for (auto & baseLocation : m_baseLocationData)
    {
        baseLocation.setPlayerOccupying(Players::Self, false);
        baseLocation.setPlayerOccupying(Players::Enemy, false);
		baseLocation.resetNumEnemyCombatUnits();
    }

    // for each unit on the map, update which base location it may be occupying

	//We start with the enemy to avoid situation with proxys locations declared as enemy region
	// update enemy base occupations
	for (const auto & kv : m_bot.UnitInfo().getUnitInfoMap(Players::Enemy))
	{
		const UnitInfo & ui = kv.second;
		if (!m_bot.Data(ui.type).isBuilding || ui.lastHealth == 0)
		{
			continue;
		}

		BaseLocation * baseLocation = getBaseLocation(ui.lastPosition);


		if (baseLocation != nullptr)
		{
			baseLocation->setPlayerOccupying(Players::Enemy, true);
		}
	}
    for (const auto & unit : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self))
    {
        // we only care about buildings on the ground
        if (!m_bot.Data(unit->unit_type).isBuilding || unit->is_flying || !unit->is_alive)
        {
            continue;
        }

        BaseLocation * baseLocation = getBaseLocation(unit->pos);

        if (baseLocation != nullptr)
        {
            baseLocation->setPlayerOccupying(Players::Self, true);
			baseLocation->setPlayerOccupying(Players::Enemy, false);
        }
    }



    // update the starting locations of the enemy player
    // this will happen one of two ways:
    
    // 1. we've seen the enemy base directly, so the baselocation will know
    if (m_playerStartingBaseLocations[Players::Enemy] == nullptr)
    {
        for (const auto & baseLocation : m_baseLocationData)
        {
            if (baseLocation.isPlayerStartLocation(Players::Enemy))
            {
                m_playerStartingBaseLocations[Players::Enemy] = &baseLocation;
            }
        }
    }
    
    // 2. we've explored every other start location and haven't seen the enemy yet
    if (m_playerStartingBaseLocations[Players::Enemy] == nullptr)
    {
        int numStartLocations = (int)getStartingBaseLocations().size();
        int numExploredLocations = 0;
        BaseLocation * unexplored = nullptr;

        for (auto & baseLocation : m_baseLocationData)
        {
            if (!baseLocation.isStartLocation())
            {
                continue;
            }

            if (baseLocation.isExplored())
            {
                numExploredLocations++;
            }
            else
            {
                unexplored = &baseLocation;
            }
        }

        // if we have explored all but one location, then the unexplored one is the enemy start location
        if (numExploredLocations == numStartLocations - 1 && unexplored != nullptr)
        {
            m_playerStartingBaseLocations[Players::Enemy] = unexplored;
            unexplored->setPlayerOccupying(Players::Enemy, true);
			
        }
    }

    // update the occupied base locations for each player
    m_occupiedBaseLocations[Players::Self] = std::set<const BaseLocation *>();
    m_occupiedBaseLocations[Players::Enemy] = std::set<const BaseLocation *>();
    for (const auto & baseLocation : m_baseLocationData)
    {
        if (baseLocation.isOccupiedByPlayer(Players::Self))
        {
            m_occupiedBaseLocations[Players::Self].insert(&baseLocation);
        }

        if (baseLocation.isOccupiedByPlayer(Players::Enemy))
        {
            m_occupiedBaseLocations[Players::Enemy].insert(&baseLocation);
        }
    }

    // We want to assign the number of enemy combat units to each base to determine which one is the safest to attack
	for (const auto & kv : m_bot.UnitInfo().getUnitInfoMap(Players::Enemy))
	{
		const UnitInfo & ui = kv.second;
		if (!Util::IsCombatUnitType(ui.type,m_bot) || ui.lastHealth == 0)
		{
			continue;
		}

		BaseLocation * baseLocation = getBaseLocation(ui.lastPosition);



		if (baseLocation != nullptr && baseLocation->isOccupiedByPlayer(Players::Enemy))
		{
			baseLocation->incrementNumEnemyCombatUnits();
		}
	}
    
}

BaseLocation * BaseLocationManager::getBaseLocation(const sc2::Point2D & pos) const
{
    if (!m_bot.Map().isValid(pos)) { return nullptr; }

    return m_tileBaseLocations[(int)pos.x][(int)pos.y];
}

void BaseLocationManager::drawBaseLocations()
{
    if (!m_bot.Config().DrawBaseLocationInfo)
    {
        return;
    }

    for (auto & baseLocation : m_baseLocationData)
    {
        baseLocation.draw();
    }

    // draw a purple sphere at the next expansion location
    sc2::Point2D nextExpansionPosition = getNextExpansion(Players::Self);

    Drawing::drawSphere(m_bot,nextExpansionPosition, 1, sc2::Colors::Purple);
    Drawing::drawText(m_bot,nextExpansionPosition, "Next Expansion Location", sc2::Colors::Purple);
}

const std::vector<const BaseLocation *> & BaseLocationManager::getBaseLocations() const
{
    return m_baseLocationPtrs;
}

const std::vector<const BaseLocation *> & BaseLocationManager::getStartingBaseLocations() const
{
    return m_startingBaseLocations;
}

const BaseLocation * BaseLocationManager::getPlayerStartingBaseLocation(int player) const
{
    return m_playerStartingBaseLocations.at(player);
}

const std::set<const BaseLocation *> & BaseLocationManager::getOccupiedBaseLocations(int player) const
{
    return m_occupiedBaseLocations.at(player);
}
const sc2::Point2D BaseLocationManager::getBuildingLocation() const
{
	return m_bot.GetStartLocation();
}
const sc2::Point2D BaseLocationManager::getRallyPoint() const
{
	//GET NEWEST EXPANSION
	sc2::Point2D newestBasePlayer = getNewestExpansion(Players::Self);

	std::vector<const BaseLocation *> startingBases = getStartingBaseLocations();
	sc2::Point2D targetPos(0.0f, 0.0f);
	for (const auto base : startingBases)
	{
		targetPos+=base->getPosition();
	}
	targetPos /= static_cast<float>(startingBases.size());
	sc2::Point2D rallyPoint = newestBasePlayer + 5*(targetPos - newestBasePlayer) / std::sqrt(Util::DistSq(targetPos - newestBasePlayer));

	// get the precomputed vector of tile positions which are sorted closes to this location
	auto & closestTorallyPoint = m_bot.Map().getClosestTilesTo(rallyPoint);
	for (size_t i(0); i < closestTorallyPoint.size(); ++i)
	{
		auto & pos = closestTorallyPoint[i];
		if (m_bot.Map().isWalkable(pos))
		{
			return pos;
		}
	}
	return rallyPoint;
}

sc2::Point2D BaseLocationManager::getNextExpansion(int player) const
{
    const BaseLocation * homeBase = getPlayerStartingBaseLocation(player);
    const BaseLocation * closestBase = nullptr;
    int minDistance = std::numeric_limits<int>::max();

	if (!homeBase)
	{
		return sc2::Point2D(0.0f, 0.0f);
	}
    for (const auto & base : getBaseLocations())
    {
        // skip mineral only and starting locations (TODO: fix this)
        if (base->isMineralOnly() || base->isOccupiedByPlayer(player))
        {
            continue;
        }

        // get the tile position of the base
        auto tile = base->getDepotPosition();
        
        bool buildingInTheWay = false; // TODO: check if there are any units on the tile

        if (buildingInTheWay)
        {
            continue;
        }

        // the base's distance from our main nexus
        int distanceFromHome = homeBase->getGroundDistance(tile);

        // if it is not connected, continue
        if (distanceFromHome < 0)
        {
            continue;
        }

        if (!closestBase || distanceFromHome < minDistance)
        {
            closestBase = base;
            minDistance = distanceFromHome;
        }
    }

    return closestBase ? closestBase->getPosition() : sc2::Point2D(0.0f, 0.0f);
}

sc2::Point2D BaseLocationManager::getNewestExpansion(int player) const
{
	const BaseLocation * homeBase = getPlayerStartingBaseLocation(player);
	const BaseLocation * newestBase = nullptr;
	float maxDistance = -1;
	for (const auto & base : getBaseLocations())
	{
		float dist = Util::Dist(homeBase->getPosition(), base->getPosition());
		if (base->isOccupiedByPlayer(Players::Self) && maxDistance < dist && base->getTownHall())
		{
			maxDistance = dist;
			newestBase = base;
		}
	}
	if (newestBase)
	{
		return newestBase->getBasePosition();
	}
	else
	{
		return sc2::Point2D(0.0f,0.0f);
	}
}

void BaseLocationManager::assignTownhallToBase(const sc2::Unit * townHall) const
{

	BaseLocation * baseLocation = getBaseLocation(townHall->pos);
	if (baseLocation)
	{
		baseLocation->setTownHall(townHall);
	}
}