#include "BaseLocation.h"
#include "Util.h"
#include "CCBot.h"
#include <sstream>
#include <iostream>

const int NearBaseLocationTileDistance = 20;

BaseLocation::BaseLocation(CCBot & bot, int baseID, const CUnits resources)
	: m_bot(bot)
	, m_centerOfResources(0.0f, 0.0f)
	, m_centerOfMinerals(0.0f, 0.0f)
	, m_centerOfBase(0.0f, 0.0f)
	, m_townhall(nullptr)
	, m_baseID(baseID)
	, m_left(std::numeric_limits<float>::max())
	, m_right(std::numeric_limits<float>::lowest())
	, m_top(std::numeric_limits<float>::lowest())
	, m_bottom(std::numeric_limits<float>::max())
	, m_isStartLocation(false)
	, m_numEnemyCombatUnits(0)
{
	m_isPlayerStartLocation[Players::Self] = false;
	m_isPlayerStartLocation[Players::Enemy] = false;
	m_isPlayerOccupying[Players::Self] = false;
	m_isPlayerOccupying[Players::Enemy] = false;

	float resourceCenterX = 0;
	float resourceCenterY = 0;
	float mineralsCenterX = 0;
	float mineralsCenterY = 0;


	// add each of the resources to its corresponding container
	for (const CUnit_ptr & resource : resources)
	{
		if (resource->isMineral())
		{
			m_minerals.push_back(resource);
			m_mineralPositions.push_back(resource->getPos());

			// add the position of the minerals to the center
			resourceCenterX += resource->getPos().x;
			resourceCenterY += resource->getPos().y;
			mineralsCenterX += resource->getPos().x;
			mineralsCenterY += resource->getPos().y;
		}
		else
		{
			m_geysers.push_back(resource);
			m_geyserPositions.push_back(resource->getPos());

			// pull the resource center toward the geyser if it exists
			resourceCenterX += resource->getPos().x;
			resourceCenterY += resource->getPos().y;
		}

		// set the limits of the base location bounding box
		const float resWidth = 1;
		const float resHeight = 0.5;

		m_left	= std::min(m_left,	resource->getPos().x - resWidth);
		m_right	= std::max(m_right,	resource->getPos().x + resWidth);
		m_top	= std::max(m_top,	resource->getPos().y + resHeight);
		m_bottom = std::min(m_bottom, resource->getPos().y - resHeight);
	}

	// calculate the center of the resources
	const size_t numResources = m_minerals.size() + m_geysers.size();
	const sc2::Point2D centerMinerals(mineralsCenterX / m_minerals.size(), mineralsCenterY / m_minerals.size());
	m_centerOfMinerals = centerMinerals;
	m_centerOfResources = sc2::Point2D(m_left + (m_right-m_left)*0.5f, m_top + (m_bottom-m_top)*0.5f);
	// compute this BaseLocation's DistanceMap, which will compute the ground distance
	// from the center of its recourses to every other tile on the map
	m_distanceMap = m_bot.Map().getDistanceMap(m_centerOfResources);
	if (m_distanceMap.getSortedTiles().size() == 1)
	{
		bool foundBetterPos = false;
		for (int k = 1; k <= 5; ++k)
		{
			for (int i = -k; i <= k; ++i)
			{
				for (int j = -k; j <= k; ++j)
				{
					if (m_bot.Map().isWalkable(m_centerOfResources + sc2::Point2D(static_cast<float>(i), static_cast<float>(j))))
					{
						m_distanceMap = m_bot.Map().getDistanceMap(m_bot.Map().getClosestWalkableTo(m_centerOfResources + sc2::Point2D(static_cast<float>(i), static_cast<float>(j))));
						foundBetterPos = true;
						break;
					}
				}
				if (foundBetterPos)
				{
					break;
				}
			}
			if (foundBetterPos)
			{
				break;
			}
		}
	}

	// if this base location position is near our own resource depot, it's our start location
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))
	{
		if (containsPosition(unit->getPos()))
		{
			m_isPlayerStartLocation[Players::Self] = true;
			m_isStartLocation = true;
			m_isPlayerOccupying[Players::Self] = true;
			m_townhall = unit;
			m_centerOfBase = unit->getPos();
			break;
		}
	}

	// check to see if this is a start location for the map
	for (const auto & pos : m_bot.Observation()->GetGameInfo().enemy_start_locations)
	{
		if (containsPosition(pos))
		{
			m_isStartLocation = true;
			m_centerOfBase = pos;
		}
	}
	// Get the CC/Nexus/hatch position
	if (!m_isStartLocation)
	{
		if (numResources == 10)
		{
			m_centerOfBase = m_centerOfResources + (m_centerOfResources - centerMinerals);
		}
		else
		{
			m_centerOfBase = m_centerOfResources;
		}
		for (const auto & pos : m_distanceMap.getSortedTiles())
		{
			if (m_bot.Map().isValid(static_cast<int>(pos.x), static_cast<int>(pos.y)) && m_bot.Map().canBuildTypeAtPosition(static_cast<int>(pos.x), static_cast<int>(pos.y), sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))
			{
				m_centerOfBase = pos;
				break;
			}
		}
	}
}

const sc2::Point2D & BaseLocation::getCenterOfMinerals() const noexcept
{
	return m_centerOfMinerals;
}

const sc2::Point2D & BaseLocation::getCenterOfBase() const noexcept
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

bool BaseLocation::isInResourceBox(int x, int y) const noexcept
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
	if (sc2::Point2D{0.0f, 0.0f} != m_centerOfBase)
	{
		return Util::DistSq(m_centerOfBase, pos) < 400 && m_bot.Map().terrainHeight(m_centerOfBase) == m_bot.Map().terrainHeight(pos);
	}
	return Util::DistSq(m_centerOfMinerals, pos) < 400 && m_bot.Map().terrainHeight(m_centerOfMinerals) == m_bot.Map().terrainHeight(pos);
}

const CUnits & BaseLocation::getGeysers() const noexcept
{
	return m_geysers;
}

const CUnits & BaseLocation::getMinerals() const noexcept
{
	return m_minerals;
}

const sc2::Point2D & BaseLocation::getCenterOfRessources() const noexcept
{
	return m_centerOfResources;
}

int BaseLocation::getGroundDistance(const sc2::Point2D & pos) const
{
	// return Util::Dist(pos, m_centerOfResources);
	// return m_bot.Query()->PathingDistance(pos, m_centerOfResources);
	// return m_distanceMap.getDistance(pos);
	const int dist = m_distanceMap.getDistance(pos);
	return dist > 0 ? dist : static_cast<int>(Util::Dist(pos, m_centerOfResources));
}

bool BaseLocation::isStartLocation() const noexcept
{
	return m_isStartLocation;
}

const std::vector<sc2::Point2D> & BaseLocation::getClosestTiles() const
{
	return m_distanceMap.getSortedTiles();
}

const CUnit_ptr BaseLocation::getTownHall() const noexcept
{
	return m_townhall;
}

void BaseLocation::setTownHall(const CUnit_ptr townHall)
{
	m_townhall = townHall;
	// it seems we have a new base. Time to update the mineral nodes.
	CUnits resources = m_bot.UnitInfo().getUnits(Players::Neutral, Util::getMineralTypes());
	m_minerals.clear();
	CUnit_ptr closestMineral = nullptr;
	float minDist = 10.0f;
	for (const auto & resource : resources)
	{
		const float dist = Util::Dist(townHall->getPos(), resource->getPos());
		if (dist < 10.0f)
		{
			m_minerals.push_back(resource);
			if (minDist > dist)
			{
				closestMineral = resource;
				minDist = dist;
			}
		}
	}
	if (closestMineral)
	{
		Micro::SmartAbility(townHall, sc2::ABILITY_ID::RALLY_BUILDING, closestMineral, m_bot);
	}
}

void BaseLocation::draw()
{
	Drawing::drawSphere(m_bot, m_centerOfResources, 1.0f, sc2::Colors::Yellow);

	std::stringstream ss;
	ss << "BaseLocation: " << m_baseID << "\n";
	ss << "Start Loc:	" << (isStartLocation() ? "true" : "false") << "\n";
	ss << "Minerals:	 " << m_mineralPositions.size() << "\n";
	ss << "Geysers:		" << m_geyserPositions.size() << "\n";
	ss << "Occupied By:	";

	if (isOccupiedByPlayer(Players::Self))
	{
		ss << "Self ";
	}

	if (isOccupiedByPlayer(Players::Enemy))
	{
		ss << "Enemy ";
	}

	ss << "\n Position:	( " << m_centerOfBase.x << " , " << m_centerOfBase.y << " )\n";

	Drawing::drawText(m_bot, sc2::Point2D(m_left, m_top+3), ss.str().c_str());
	Drawing::drawText(m_bot, sc2::Point2D(m_left, m_bottom), ss.str().c_str());

	// draw the base bounding box
	Drawing::drawBox(m_bot, m_left, m_top, m_right, m_bottom);

	for (float x=m_left; x < m_right; ++x)
	{
		Drawing::drawLine(m_bot, x, m_top, x, m_bottom, sc2::Color(160, 160, 160));
	}

	for (float y = m_bottom; y < m_top; ++y)
	{
		Drawing::drawLine(m_bot, m_left, y, m_right, y, sc2::Color(160, 160, 160));
	}

	for (const auto & mineralPos : m_mineralPositions)
	{
		Drawing::drawSphere(m_bot, mineralPos, 1.0f, sc2::Colors::Teal);
	}

	for (const auto & geyserPos : m_geyserPositions)
	{
		Drawing::drawSphere(m_bot, geyserPos, 1.0f, sc2::Colors::Green);
	}

	if (m_isStartLocation)
	{
		Drawing::drawSphere(m_bot, m_centerOfBase, 1.0f, sc2::Colors::Red);
	}

	const float ccWidth = 5;
	const float ccHeight = 4;
	const sc2::Point2D boxtl = m_centerOfBase - sc2::Point2D(ccWidth/2, -ccHeight/2);
	const sc2::Point2D boxbr = m_centerOfBase + sc2::Point2D(ccWidth/2, -ccHeight/2);

	Drawing::drawBox(m_bot, boxtl.x, boxtl.y, boxbr.x, boxbr.y, sc2::Colors::Red);

	const sc2::Point2D posEnd = getCenterOfRessources() + 1.2f*(getCenterOfRessources() - getCenterOfBase());
	Drawing::drawLine(m_bot, m_bot.Map().getClosestBorderPoint(posEnd, 0), posEnd);
	// m_distanceMap.draw(m_bot);
}

bool BaseLocation::isMineralOnly() const
{
	return getGeysers().empty();
}

void BaseLocation::resetNumEnemyCombatUnits() noexcept
{
	m_numEnemyCombatUnits = 0;
}

void BaseLocation::incrementNumEnemyCombatUnits()
{
	++m_numEnemyCombatUnits;
}

/*
const int BaseLocation::getNumEnemyCombatUnits() const
{
	return m_numEnemyCombatUnits;
}
*/

const int BaseLocation::getNumEnemyStaticD() const
{
	int threadLvl = 0;
	CUnits pfs = m_bot.UnitInfo().getUnits(Players::Enemy, sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS);
	for (const auto & pf : pfs)
	{
		if (pf->isCompleted() && containsPosition(pf->getPos()))
		{
			threadLvl+=3;
		}
	}
	CUnits staticD = m_bot.UnitInfo().getUnits(Players::Enemy, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED, sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET }));
	for (const auto & d : staticD)
	{
		if (d->isCompleted() && containsPosition(d->getPos()))
		{
			++threadLvl;
		}
	}
	return threadLvl;
}

const int BaseLocation::getBaseID() const
{
	return m_baseID;
}
