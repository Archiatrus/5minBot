#include "BaseLocation.h"
#include "Util.h"
#include "CCBot.h"
#include <sstream>
#include <iostream>

const int NearBaseLocationTileDistance = 20;

BaseLocation::BaseLocation(CCBot & bot, int baseID, const std::vector<const sc2::Unit *> & resources)
	: m_bot(bot)
	, m_baseID(baseID)
	, m_isStartLocation(false)
	, m_left(std::numeric_limits<float>::max())
	, m_right(std::numeric_limits<float>::lowest())
	, m_top(std::numeric_limits<float>::lowest())
	, m_bottom(std::numeric_limits<float>::max())
	, m_numEnemyCombatUnits(0)
{
    m_isPlayerStartLocation[0] = false;
    m_isPlayerStartLocation[1] = false;
    m_isPlayerOccupying[0] = false;
    m_isPlayerOccupying[1] = false;

	float resourceCenterX = 0;
	float resourceCenterY = 0;
	float mineralsCenterX = 0;
	float mineralsCenterY = 0;


    // add each of the resources to its corresponding container
    for (const auto & resource : resources)
    {
        if (Util::IsMineral(resource))
        {
            m_minerals.push_back(resource);
            m_mineralPositions.push_back(resource->pos);

            // add the position of the minerals to the center
            resourceCenterX += resource->pos.x;
            resourceCenterY += resource->pos.y;
			mineralsCenterX += resource->pos.x;
			mineralsCenterY += resource->pos.y;
        }
        else
        {
            m_geysers.push_back(resource);
            m_geyserPositions.push_back(resource->pos);

            // pull the resource center toward the geyser if it exists
            resourceCenterX += resource->pos.x;
            resourceCenterY += resource->pos.y;
        }

        // set the limits of the base location bounding box
        float resWidth = 1;
        float resHeight = 0.5;

        m_left   = std::min(m_left,   resource->pos.x - resWidth);
        m_right  = std::max(m_right,  resource->pos.x + resWidth);
        m_top    = std::max(m_top,    resource->pos.y + resHeight);
        m_bottom = std::min(m_bottom, resource->pos.y - resHeight);
    }

    // calculate the center of the resources
    size_t numResources = m_minerals.size() + m_geysers.size();
	sc2::Point2D centerMinerals(mineralsCenterX / m_minerals.size(), mineralsCenterY / m_minerals.size());
    m_centerOfResources = sc2::Point2D(m_left + (m_right-m_left)*0.5f, m_top + (m_bottom-m_top)*0.5f);
	m_centerOfBase = m_centerOfResources + (m_centerOfResources - centerMinerals);

    // compute this BaseLocation's DistanceMap, which will compute the ground distance
    // from the center of its recourses to every other tile on the map
    m_distanceMap = m_bot.Map().getDistanceMap(m_centerOfResources);

    // check to see if this is a start location for the map
    for (const auto & pos : m_bot.Observation()->GetGameInfo().enemy_start_locations)
    {
        if (containsPosition(pos))
        {
            m_isStartLocation = true;
            m_depotPosition = pos;
        }
    }
    
    // if this base location position is near our own resource depot, it's our start location
    for (const auto & unit : m_bot.Observation()->GetUnits())
    {
        if (Util::GetPlayer(unit) == Players::Self && Util::IsTownHall(unit) && containsPosition(unit->pos))
        {
            m_isPlayerStartLocation[Players::Self] = true;
            m_isStartLocation = true;
            m_isPlayerOccupying[Players::Self] = true;
			m_townhall = unit;
            break;
        }
    }
    
    // if it's not a start location, we need to calculate the depot position
    if (!isStartLocation())
    {
        // the position of the depot will be the closest spot we can build one from the resource center
        for (const auto & tile : getClosestTiles())
        {
            // TODO: m_depotPosition = depot position for this base location
        }
    }
}

// TODO: calculate the actual depot position
const sc2::Point2D & BaseLocation::getDepotPosition() const
{
    return getPosition();
}

const sc2::Point2D & BaseLocation::getBasePosition() const
{
	return m_centerOfBase;
}

void BaseLocation::setPlayerOccupying(int player, bool occupying)
{
    m_isPlayerOccupying[player] = occupying;

    // if this base is a start location that's occupied by the enemy, it's that enemy's start location
    if (occupying && player == Players::Enemy && isStartLocation() && m_isPlayerStartLocation[player] == false)
    {
        m_isPlayerStartLocation[player] = true;
    }
}

bool BaseLocation::isInResourceBox(int x, int y) const
{
    return x >= m_left && x < m_right && y < m_top && y >= m_bottom;
}

bool BaseLocation::isOccupiedByPlayer(int player) const
{
    return m_isPlayerOccupying.at(player);
}

bool BaseLocation::isExplored() const
{
    return m_bot.Map().isExplored(m_centerOfBase);
}

bool BaseLocation::isPlayerStartLocation(int player) const
{
    return m_isPlayerStartLocation.at(player);
}

bool BaseLocation::containsPosition(const sc2::Point2D & pos) const
{
    if (!m_bot.Map().isValid(pos) || (pos.x == 0 && pos.y == 0))
    {
        return false;
    }

    return getGroundDistance(pos) < NearBaseLocationTileDistance;
}

const std::vector<const sc2::Unit *> & BaseLocation::getGeysers() const
{
    return m_geysers;
}

const std::vector<const sc2::Unit *> & BaseLocation::getMinerals() const
{
    return m_minerals;
}

const sc2::Point2D & BaseLocation::getPosition() const
{
    return m_centerOfResources;
}

int BaseLocation::getGroundDistance(const sc2::Point2D & pos) const
{
    //return Util::Dist(pos, m_centerOfResources);
    return m_distanceMap.getDistance(pos);
}

bool BaseLocation::isStartLocation() const
{
    return m_isStartLocation;
}

const std::vector<sc2::Point2D> & BaseLocation::getClosestTiles() const
{
    return m_distanceMap.getSortedTiles();
}

const sc2::Unit * BaseLocation::getTownHall() const
{
	return m_townhall;
}

void BaseLocation::setTownHall(const sc2::Unit * townHall)
{
	m_townhall = townHall;
	//it seems we have a new base. Time to update the mineral nodes.
	sc2::Units resources = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral, sc2::IsUnits(Util::getMineralTypes()));
	m_minerals.clear();
	for (const auto & resource : resources)
	{
		if (Util::Dist(townHall->pos, resource->pos)<10)
		{
			m_minerals.push_back(resource);
		}
	}
	auto minerals = Util::getClostestMineral(townHall->pos, m_bot);
	if (minerals)
	{
		m_bot.Actions()->UnitCommand(townHall, sc2::ABILITY_ID::RALLY_BUILDING, minerals);
	}
}

void BaseLocation::draw()
{
    Drawing::drawSphere(m_bot,m_centerOfResources, 1.0f, sc2::Colors::Yellow);

    std::stringstream ss;
    ss << "BaseLocation: " << m_baseID << "\n";
    ss << "Start Loc:    " << (isStartLocation() ? "true" : "false") << "\n";
    ss << "Minerals:     " << m_mineralPositions.size() << "\n";
    ss << "Geysers:      " << m_geyserPositions.size() << "\n";
    ss << "Occupied By:  ";

    if (isOccupiedByPlayer(Players::Self))
    {
        ss << "Self ";
    }

    if (isOccupiedByPlayer(Players::Enemy))
    {
        ss << "Enemy ";
    }

    Drawing::drawText(m_bot,sc2::Point2D(m_left, m_top+3), ss.str().c_str());
    Drawing::drawText(m_bot,sc2::Point2D(m_left, m_bottom), ss.str().c_str());

    // draw the base bounding box
    Drawing::drawBox(m_bot,m_left, m_top, m_right, m_bottom);

    for (float x=m_left; x < m_right; ++x)
    {
        Drawing::drawLine(m_bot,x, m_top, x, m_bottom, sc2::Color(160, 160, 160));
    }

    for (float y=m_bottom; y<m_top; ++y)
    {
        Drawing::drawLine(m_bot,m_left, y, m_right, y, sc2::Color(160, 160, 160));
    }

    for (const auto & mineralPos : m_mineralPositions)
    {
        Drawing::drawSphere(m_bot,mineralPos, 1.0f, sc2::Colors::Teal);
    }

    for (const auto & geyserPos : m_geyserPositions)
    {
        Drawing::drawSphere(m_bot,geyserPos, 1.0f, sc2::Colors::Green);
    }

    if (m_isStartLocation)
    {
        Drawing::drawSphere(m_bot,m_depotPosition, 1.0f, sc2::Colors::Red);
    }
    
    float ccWidth = 5;
    float ccHeight = 4;
    sc2::Point2D boxtl = m_depotPosition - sc2::Point2D(ccWidth/2, -ccHeight/2);
    sc2::Point2D boxbr = m_depotPosition + sc2::Point2D(ccWidth/2, -ccHeight/2);

    Drawing::drawBox(m_bot,boxtl.x, boxtl.y, boxbr.x, boxbr.y, sc2::Colors::Red);

    //m_distanceMap.draw(m_bot);
}

bool BaseLocation::isMineralOnly() const
{
    return getGeysers().empty();
}

void BaseLocation::resetNumEnemyCombatUnits()
{
	m_numEnemyCombatUnits = 0;
}

void BaseLocation::incrementNumEnemyCombatUnits()
{
	++m_numEnemyCombatUnits;
}

const int BaseLocation::getNumEnemyCombatUnits() const
{
	return m_numEnemyCombatUnits;
}