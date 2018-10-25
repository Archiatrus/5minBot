#include "MapTools.h"
#include "Util.h"
#include "CCBot.h"
#include "Drawing.h"
#include "pathPlaning.h"
#include <iostream>
#include <sstream>
#include <fstream>

const size_t LegalActions = 4;
const int actionX[LegalActions] ={1, -1, 0, 0};
const int actionY[LegalActions] ={0, 0, 1, -1};

typedef std::vector<std::vector<bool>> vvb;
typedef std::vector<std::vector<int>>  vvi;
typedef std::vector<std::vector<float>>  vvf;

// constructor for MapTools
MapTools::MapTools(CCBot & bot)
	: m_bot(bot)
	, m_width(0)
	, m_height(0)
	, m_maxZ(0.0f)
	, m_frame(0)
    // , overseerMap()
{
}

void MapTools::onStart()
{
	m_width  = m_bot.Observation()->GetGameInfo().width;
	m_height = m_bot.Observation()->GetGameInfo().height;

	m_walkable	   = vvb(m_width, std::vector<bool>(m_height, true));
	m_buildable	  = vvb(m_width, std::vector<bool>(m_height, false));
	m_ramp = vvb(m_width, std::vector<bool>(m_height, false));
	m_lastSeen	   = vvi(m_width, std::vector<int>(m_height, 0));
	m_sectorNumber   = vvi(m_width, std::vector<int>(m_height, 0));
	m_terrainHeight  = vvf(m_width, std::vector<float>(m_height, 0.0f));
	// Set the boolean grid data from the Map
	for (size_t x(0); x < m_width; ++x)
	{
		for (size_t y(0); y < m_height; ++y)
		{
			m_buildable[x][y]   = Util::Placement(m_bot.Observation()->GetGameInfo(), sc2::Point2D(x+0.5f, y+0.5f));
			m_walkable[x][y]	= m_buildable[x][y] || Util::Pathable(m_bot.Observation()->GetGameInfo(), sc2::Point2D(x+0.5f, y+0.5f));
			m_terrainHeight[x][y] = m_bot.Observation()->TerrainHeight(sc2::Point2D(x + 0.5f, y + 0.5f));  // Util::TerainHeight(m_bot.Observation()->GetGameInfo(), sc2::Point2D(x + 0.5f, y + 0.5f));
			m_ramp[x][y] = m_walkable[x][y] || !m_buildable[x][y];
		}
	}
	for (const auto & unit : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral))
	{
		m_maxZ = std::max(unit->pos.z, m_maxZ);
	}
	// RedShift..... -.-
	sc2::Units minerals = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral, sc2::IsUnits(Util::getMineralTypes()));
	for (const auto & mineral : minerals)
	{
		for (int i = -1; i <= 1; ++i)
		{
			for (int j = -1; j <= 1; ++j)
			{
				m_walkable[static_cast<int>(mineral->pos.x)+i][static_cast<int>(mineral->pos.y)+j] = false;
			}
		}
	}
	computeConnectivity();

	// overseerMap.setBot(&m_bot); //ADD THIS LINE (OVERSEER)
	// overseerMap.Initialize(); //ADD THIS LINE (OVERSEER)
    // Drawing::cout{} << "Number of tiles on map: " << overseerMap.size() << std::endl;  // ADD THIS LINE (OVERSEER)
    // Drawing::cout{} << "Number of regions: " << overseerMap.getRegions().size() << std::endl;  // ADD THIS LINE (OVERSEER)
}

void MapTools::onFrame()
{
	m_frame++;
	return;
}

void MapTools::computeConnectivity()
{
	// the fringe data structe we will use to do our BFS searches
	std::vector<sc2::Point2D> fringe;
	fringe.reserve(static_cast<size_t>(m_width) * static_cast<size_t>(m_height));
	int sectorNumber = 0;

	// for every tile on the map, do a connected flood fill using BFS
    for (int x=0; x < m_width; ++x)
	{
        for (int y=0; y < m_height; ++y)
		{
			// if the sector is not currently 0, or the map isn't walkable here, then we can skip this tile
			if (getSectorNumber(x, y) != 0 || !isWalkable(x, y))
			{
				continue;
			}

			// increase the sector number, so that walkable tiles have sectors 1-N
			sectorNumber++;

			// reset the fringe for the search and add the start tile to it
			fringe.clear();
			fringe.push_back(sc2::Point2D(x+0.5f, y+0.5f));
			m_sectorNumber[x][y] = sectorNumber;

			// do the BFS, stopping when we reach the last element of the fringe
			for (size_t fringeIndex=0; fringeIndex < fringe.size(); ++fringeIndex)
			{
				auto & tile = fringe[fringeIndex];

				// check every possible child of this tile
				for (size_t a = 0; a < LegalActions; ++a)
				{
					sc2::Point2D nextTile(tile.x + actionX[a], tile.y + actionY[a]);

					// if the new tile is inside the map bounds, is walkable, and has not been assigned a sector, add it to the current sector and the fringe
					if (isValid(nextTile) && isWalkable(nextTile) && (getSectorNumber(nextTile) == 0))
					{
						m_sectorNumber[static_cast<int>(nextTile.x)][static_cast<int>(nextTile.y)] = sectorNumber;
						fringe.push_back(nextTile);
					}
				}
			}
		}
	}
}

bool MapTools::isExplored(const sc2::Point2D & pos) const
{
	if (!isValid(pos)) { return false; }

	sc2::Visibility vis = m_bot.Observation()->GetVisibility(pos);
	return vis == sc2::Visibility::Fogged || vis == sc2::Visibility::Visible;
}

bool MapTools::isVisible(const sc2::Point2D & pos) const
{
	if (!isValid(pos)) { return false; }

	return m_bot.Observation()->GetVisibility(pos) == sc2::Visibility::Visible;
}

bool MapTools::isPowered(const sc2::Point2D & pos) const
{
	for (const auto & powerSource : m_bot.Observation()->GetPowerSources())
	{
		if (Util::Dist(pos, powerSource.position) < powerSource.radius)
		{
			return true;
		}
	}

	return false;
}

float MapTools::terrainHeight(float x, float y) const
{
	if (!isValid(static_cast<int>(x), static_cast<int>(y)))
	{
		return -1.0f;
	}
	return m_terrainHeight[static_cast<int>(x)][static_cast<int>(y)];
}

float MapTools::terrainHeight(sc2::Point2D pos) const
{
	if (!isValid(pos))
	{
		return -1.0f;
	}
	return m_terrainHeight[static_cast<int>(pos.x)][static_cast<int>(pos.y)];
}

int MapTools::getGroundDistance(const sc2::Point2D & src, const sc2::Point2D & dest) const
{
	if (_allMaps.size() > 50)
	{
		_allMaps.clear();
	}

	return getDistanceMap(dest).getDistance(src);
}

const DistanceMap & MapTools::getDistanceMap(const sc2::Point2D & tile) const
{
	std::pair<int, int> intTile(static_cast<int>(tile.x), static_cast<int>(tile.y));

	if (_allMaps.find(intTile) == _allMaps.end())
	{
		_allMaps[intTile] = DistanceMap();
		_allMaps[intTile].computeDistanceMap(m_bot, tile);
	}

	return _allMaps[intTile];
}

const DistanceMap & MapTools::getAirDistanceMap(const sc2::Point2D & tile) const
{
	std::pair<int, int> intTile(static_cast<int>(tile.x), static_cast<int>(tile.y));

	if (_allAirMaps.find(intTile) == _allAirMaps.end())
	{
		_allAirMaps[intTile] = DistanceMap();
		_allAirMaps[intTile].computeAirDistanceMap(m_bot, tile);
	}

	return _allAirMaps[intTile];
}

int MapTools::getSectorNumber(int x, int y) const
{
	if (!isValid(x, y))
	{
		return 0;
	}

	return m_sectorNumber[x][y];
}

int MapTools::getSectorNumber(const sc2::Point2D & pos) const
{
	return getSectorNumber(static_cast<int>(pos.x), static_cast<int>(pos.y));
}

bool MapTools::isValid(int x, int y) const
{
    return x >= 0 && y >= 0 && x < static_cast<int>(m_width) && y < static_cast<int>(m_height);
}

bool MapTools::isValid(const sc2::Point2D & pos) const
{
	return isValid(static_cast<int>(pos.x), static_cast<int>(pos.y));
}



bool MapTools::isConnected(int x1, int y1, int x2, int y2) const
{
	if (!isValid(x1, y1) || !isValid(x2, y2))
	{
		return false;
	}

	int s1 = getSectorNumber(x1, y1);
	int s2 = getSectorNumber(x2, y2);

	return s1 != 0 && (s1 == s2);
}

bool MapTools::isConnected(const sc2::Point2D & p1, const sc2::Point2D & p2) const
{
	return isConnected(static_cast<int>(p1.x), static_cast<int>(p1.y), static_cast<int>(p2.x), static_cast<int>(p2.y));
}

bool MapTools::isBuildable(int x, int y) const
{
	if (!isValid(x, y))
	{
		return false;
	}

	return m_buildable[x][y];
}

bool MapTools::canBuildTypeAtPosition(float x, float y, sc2::UnitTypeID type) const
{
	return m_bot.Query()->Placement(m_bot.Data(type).buildAbility, sc2::Point2D(x, y));
}

bool MapTools::isBuildable(const sc2::Point2D & tile) const
{
	return isBuildable(static_cast<int>(tile.x), static_cast<int>(tile.y));
}

bool MapTools::isDepotBuildableTile(const sc2::Point2D & tile) const
{
	if (!isValid(tile))
	{
		return false;
	}

	return m_ramp[static_cast<int>(tile.x)][static_cast<int>(tile.y)];
}

bool MapTools::isWalkable(int x, int y) const
{
	return isValid(x, y) && m_walkable[x][y];
}

bool MapTools::isWalkable(const sc2::Point2D & tile) const
{
	return isWalkable(static_cast<int>(tile.x), static_cast<int>(tile.y));
}

int MapTools::width() const
{
	return static_cast<int>(m_width);
}

int MapTools::height() const
{
	return static_cast<int>(m_height);
}

const std::vector<sc2::Point2D> & MapTools::getClosestTilesTo(const sc2::Point2D & pos) const
{
	return getDistanceMap(pos).getSortedTiles();
}

const std::vector<sc2::Point2D> & MapTools::getClosestTilesToAir(const sc2::Point2D & pos) const
{
	return getAirDistanceMap(pos).getSortedTilesAir();
}


const sc2::Point2D MapTools::getClosestWalkableTo(const sc2::Point2D & pos) const
{
	// get the precomputed vector of tile positions which are sorted closes to this location
	// iterate through the list until we've found a suitable location
	sc2::Point2D validPos = { std::max(0.0f, std::min(pos.x, static_cast<float>(m_width))), std::max(0.0f, std::min(pos.y, static_cast<float>(m_height))) };
	for (const auto & closestToPos : getClosestTilesTo(validPos))
	{
		if (isWalkable(closestToPos))
		{
			return closestToPos;
		}
	}
	return sc2::Point2D(0, 0);
}

sc2::Point2D MapTools::getLeastRecentlySeenPosition() const
{
	int minSeen = std::numeric_limits<int>::max();
	sc2::Point2D leastSeen(0.0f, 0.0f);
	const BaseLocation * baseLocation = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);

	for (const auto & tile : baseLocation->getClosestTiles())
	{
		BOT_ASSERT(isValid(tile), "How is this tile not valid?");

		int lastSeen = m_lastSeen[static_cast<int>(tile.x)][static_cast<int>(tile.y)];
		if (lastSeen < minSeen)
		{
			minSeen = lastSeen;
			leastSeen = tile;
		}
	}

	return leastSeen;
}

sc2::Point2D MapTools::getRampPoint(const BaseLocation * base) const
{
	const sc2::Point2D startPoint = base->getCenterOfBase() + Util::normalizeVector(base->getCenterOfBase() - base->getCenterOfMinerals(), 5.0f);
	const float startHeight = m_bot.Observation()->TerrainHeight(startPoint);

	sc2::Point2D currentPos = sc2::Point2D(std::round(startPoint.x) + 0.5f, std::round(startPoint.y) + 0.5f);
	const sc2::Point2D enemyPoint = m_bot.Observation()->GetGameInfo().enemy_start_locations.front();
	BaseLocation * const enemyBaseLocation = m_bot.Bases().getBaseLocation(enemyPoint);
	const float stepSize = 1.0;
	const sc2::Point2D xMove(stepSize, 0.0f);
	const sc2::Point2D yMove(0.0f, stepSize);
	int currentWalkingDistance = enemyBaseLocation->getGroundDistance(startPoint);
	bool foundNewPos = true;
	while (foundNewPos)
	{
		foundNewPos = false;
		for (float i = -1.0f; i <= 1.0f; ++i)
		{
			for (float j = -1.0f; j <= 1.0f; ++j)
			{
				if (i != 0.0f || j != 0.0f)
				{
					const sc2::Point2D newPos = currentPos + i*xMove + j*yMove;
					const int dist = enemyBaseLocation->getGroundDistance(newPos);
					if (m_bot.Observation()->TerrainHeight(newPos) == startHeight && dist > 0 && m_bot.Observation()->IsPathable(newPos) && (m_bot.Observation()->IsPlacable(currentPos) || m_bot.Observation()->IsPlacable(newPos)))
					{
						if ((m_bot.Observation()->IsPlacable(newPos + sc2::Point2D(0.0f, 1.0f)) || m_bot.Observation()->IsPlacable(newPos - sc2::Point2D(0.0f, 1.0f)))
							&& (m_bot.Observation()->IsPlacable(newPos + sc2::Point2D(1.0f, 0.0f)) || m_bot.Observation()->IsPlacable(newPos - sc2::Point2D(1.0f, 0.0f))))
						{
							bool newPosBetter = false;
							if (currentWalkingDistance > dist)  // easy
							{
								newPosBetter = true;
							}
							else if (currentWalkingDistance == dist && m_bot.Observation()->IsPlacable(currentPos))  // Now it gets complicated
							{
								if (!m_bot.Observation()->IsPlacable(newPos))
								{
									newPosBetter = true;
								}
								else
								{
									const std::vector<float> dists = m_bot.Query()->PathingDistance({ { sc2::NullTag, currentPos, enemyBaseLocation->getCenterOfMinerals() }, { sc2::NullTag, newPos, enemyBaseLocation->getCenterOfMinerals() } });
									if (dists.front() > dists.back())
									{
										newPosBetter = true;
									}
								}
							}
							if (newPosBetter)
							{
								currentWalkingDistance = dist;
								currentPos = newPos;
								foundNewPos = true;
								break;
							}
						}
					}
				}
			}
			if (foundNewPos)
			{
				break;
			}
		}
	}
	if (Util::Dist(startPoint, currentPos) < 20.0f)
	{
		return currentPos;
	}
	else
	{
		return sc2::Point2D(0.0f, 0.0f);
	}
}

sc2::Point2D MapTools::getWallPositionBunker() const
{
	if (hasPocketBase())
	{
		return sc2::Point2D{0.0f, 0.0f};
	}
	sc2::Point2D rampPoint = getRampPoint(m_bot.Bases().getNaturalExpansion(Players::Self));
	if (rampPoint == sc2::Point2D{ 0.0f, 0.0f })
	{
		return rampPoint;
	}
	int rampType = 0;
	if (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 0, 1 }))  // North
	{
		rampType += 10;
	}
	if (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1, 0 }))  // East
	{
		rampType += 1;
	}
	sc2::Point2D bunkerPosition = sc2::Point2D{0.0f, 0.0f};
	switch (rampType)
	{
	case(0):  // SW
	{
		while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1.0f, -1.0f }))
		{
			rampPoint += sc2::Point2D{ 1.0f, -1.0f };
		}
		int rampLength = 1;
		while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ 1.0f, -1.0f }))
		{
			++rampLength;
		}
		if (rampLength == 6)
		{
			bunkerPosition = rampPoint + sc2::Point2D(-1.0f, 4.0f);
		}
		break;
	}
	case(1):  // SE
	{
		while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1.0f, 1.0f }))
		{
			rampPoint += sc2::Point2D{ 1.0f, 1.0f };
		}
		int rampLength = 1;
		while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ 1.0f, 1.0f }))
		{
			++rampLength;
		}
		if (rampLength == 4)
		{
			bunkerPosition = rampPoint + sc2::Point2D(-4.0f, 1.0f);
		}
		else if (rampLength == 6)
		{
			bunkerPosition = rampPoint + sc2::Point2D(-4.0f, -1.0f);
		}
		break;
	}
	case(10):  // NW
	{
		while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ -1.0f, -1.0f }))
		{
			rampPoint += sc2::Point2D{ -1.0f, -1.0f };
		}
		int rampLength = 1;
		while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ -1.0f, -1.0f }))
		{
			++rampLength;
		}
		if (rampLength == 4)
		{
			bunkerPosition = rampPoint + sc2::Point2D(4.0f, -1.0f);
		}
		else if (rampLength == 6)
		{
			bunkerPosition = rampPoint + sc2::Point2D(4.0f, 1.0f);
		}
		break;
	}
	case(11):  // NE
	{
		while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ -1.0f, 1.0f }))
		{
			rampPoint += sc2::Point2D{ -1.0f, 1.0f };
		}
		int rampLength = 1;
		while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ -1.0f, 1.0f }))
		{
			++rampLength;
		}
		if (rampLength == 6)
		{
			bunkerPosition = rampPoint + sc2::Point2D(1.0f, -4.0f);
		}
		break;
	}
	}
	return bunkerPosition;
}

sc2::Point2D MapTools::getWallPositionDepot() const
{
	sc2::Point2D firstWall = getWallPositionDepot(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self));
	if (firstWall == sc2::Point2D{0.0f, 0.0f} && !hasPocketBase())
	{
		return getWallPositionDepot(m_bot.Bases().getNaturalExpansion(Players::Self));
	}
	return firstWall;
}

sc2::Point2D MapTools::getWallPositionDepot(const BaseLocation * base) const
{
	if (!base || !base->getTownHall() || !base->getTownHall()->isAlive())
	{
		return sc2::Point2D{ 0.0f, 0.0f };
	}
	sc2::Point2D rampPoint = getRampPoint(base);
	if (rampPoint == sc2::Point2D{ 0.0f, 0.0f })
	{
		return rampPoint;
	}
	int rampType = 0;
	if (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 0, 1 }))  // North
	{
		rampType += 10;
	}
	if (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1, 0 }))  // East
	{
		rampType += 1;
	}
	std::vector<sc2::Point2D> positions;
	switch (rampType)
	{
		case(0):  // SW
		{
			while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1.0f, -1.0f }))
			{
				rampPoint += sc2::Point2D{ 1.0f, -1.0f };
			}
			int rampLength = 1;
			while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ 1.0f, -1.0f }))
			{
				++rampLength;
			}
			if (rampLength == 2)
			{
				positions = { rampPoint + sc2::Point2D(0.5f, 1.5f), rampPoint + sc2::Point2D(1.5f, -0.5f), rampPoint + sc2::Point2D(-1.5f, 2.5f) };
			}
			else if (rampLength == 6)
			{
				positions = { rampPoint + sc2::Point2D(0.5f, 1.5f), rampPoint + sc2::Point2D(1.5f, -0.5f), rampPoint + sc2::Point2D(-3.5f, 5.5f), rampPoint + sc2::Point2D(-5.5f, 6.5f) };
			}
			break;
		}
		case(1):  // SE
		{
			while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ 1.0f, 1.0f }))
			{
				rampPoint += sc2::Point2D{ 1.0f, 1.0f };
			}
			int rampLength = 1;
			while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ 1.0f, 1.0f }))
			{
				++rampLength;
			}
			if (rampLength == 2)
			{
				positions = { rampPoint + sc2::Point2D(0.5f, 1.5f), rampPoint + sc2::Point2D(-1.5f, 0.5f), rampPoint + sc2::Point2D(-2.5f, -1.5f) };
			}
			else if (rampLength == 4)
			{
				positions = { rampPoint + sc2::Point2D(0.5f, 1.5f), rampPoint + sc2::Point2D(-1.5f, 0.5f), rampPoint + sc2::Point2D(-3.5f, -1.5f), rampPoint + sc2::Point2D(-4.5f, -3.5f) };
			}
			else if (rampLength == 6)
			{
				positions = { rampPoint + sc2::Point2D(0.5f, 1.5f), rampPoint + sc2::Point2D(-1.5f, 0.5f), rampPoint + sc2::Point2D(-5.5f, -3.5f), rampPoint + sc2::Point2D(-6.5f, -5.5f) };
			}
			break;
		}
		case(10):  // NW
		{
			while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ -1.0f, -1.0f }))
			{
				rampPoint += sc2::Point2D{ -1.0f, -1.0f };
			}
			int rampLength = 1;
			while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ -1.0f, -1.0f }))
			{
				++rampLength;
			}
			if (rampLength == 2)
			{
				positions = { rampPoint + sc2::Point2D(-0.5f, -1.5f), rampPoint + sc2::Point2D(1.5f, -0.5f), rampPoint + sc2::Point2D(2.5f, 1.5f) };
			}
			else if (rampLength == 4)
			{
				positions = { rampPoint + sc2::Point2D(-0.5f, -1.5f), rampPoint + sc2::Point2D(1.5f, -0.5f), rampPoint + sc2::Point2D(3.5f, 1.5f), rampPoint + sc2::Point2D(4.5f, 3.5f) };
			}
			else if (rampLength == 6)
			{
				positions = { rampPoint + sc2::Point2D(-0.5f, -1.5f), rampPoint + sc2::Point2D(1.5f, -0.5f), rampPoint + sc2::Point2D(5.5f, 3.5f), rampPoint + sc2::Point2D(6.5f, 5.5f) };
			}
			break;
		}
		case(11):  // NE
		{
			while (!m_bot.Observation()->IsPlacable(rampPoint + sc2::Point2D{ -1.0f, 1.0f }))
			{
				rampPoint += sc2::Point2D{ -1.0f, 1.0f };
			}
			int rampLength = 1;
			while (!m_bot.Observation()->IsPlacable(rampPoint - static_cast<float>(rampLength)*sc2::Point2D{ -1.0f, 1.0f }))
			{
				++rampLength;
			}
			if (rampLength == 2)
			{
				positions = { rampPoint + sc2::Point2D(-0.5f, -1.5f), rampPoint + sc2::Point2D(-1.5f, 0.5f), rampPoint + sc2::Point2D(1.5f, -2.5f) };
			}
			if (rampLength == 6)
			{
				positions = { rampPoint + sc2::Point2D(-0.5f, -1.5f), rampPoint + sc2::Point2D(-1.5f, 0.5f), rampPoint + sc2::Point2D(3.5f, -5.5f), rampPoint + sc2::Point2D(5.5f, -6.5f) };
			}
			break;
		}
	}
	const sc2::ABILITY_ID depotID = sc2::ABILITY_ID::BUILD_SUPPLYDEPOT;
	std::vector<sc2::QueryInterface::PlacementQuery> placementBatched;
	for (const auto & pos : positions)
	{
		placementBatched.push_back({ depotID, pos });
	}
	std::vector<bool> result = m_bot.Query()->Placement(placementBatched);
    for (size_t i = 0; i < result.size(); ++i)
	{
		if (result[i])
		{
			return positions[i];
		}
	}
	return sc2::Point2D{0.0f, 0.0f};
}

sc2::Point2D MapTools::getBunkerPosition() const
{
	if (m_bot.Map().hasPocketBase())
	{
		const sc2::Point2D startPoint(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase());
		const float startHeight = m_bot.Observation()->TerrainHeight(startPoint);
		sc2::Point2D currentPos = startPoint;
		const sc2::Point2D enemyPoint = m_bot.Observation()->GetGameInfo().enemy_start_locations.front();
		BaseLocation * const enemyBaseLocation = m_bot.Bases().getBaseLocation(enemyPoint);
		const float stepSize = 2.0;
		const sc2::Point2D xMove(stepSize, 0.0f);
		const sc2::Point2D yMove(0.0f, stepSize);
		int currentWalkingDistance = enemyBaseLocation->getGroundDistance(startPoint);
		bool foundNewPos = true;
		while (foundNewPos)
		{
			foundNewPos = false;
			for (float i = -1.0f; i <= 1.0f; ++i)
			{
				for (float j = -1.0f; j <= 1.0f; ++j)
				{
					if (i != 0.0f || j != 0.0f)
					{
						const sc2::Point2D newPos = currentPos + i*xMove + j*yMove;
						const int dist = enemyBaseLocation->getGroundDistance(newPos);
						if (m_bot.Observation()->TerrainHeight(newPos) == startHeight && dist > 0 && currentWalkingDistance > dist)
						{
							currentWalkingDistance = dist;
							currentPos = newPos;
							foundNewPos = true;
							break;
						}
					}
				}
				if (foundNewPos)
				{
					break;
				}
			}
		}
		return currentPos;
	}
	else
	{
		sc2::Point2D bPoint = m_bot.Map().getWallPositionBunker();
		if (bPoint == sc2::Point2D{ 0.0f, 0.0f })
		{
			if (m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes()).size() == 1)
			{
				sc2::Point2D fixpoint = m_bot.Bases().getNextExpansion(Players::Self);

				std::vector<const BaseLocation *> startingBases = m_bot.Bases().getStartingBaseLocations();
				sc2::Point2D targetPos(0.0f, 0.0f);
				for (const auto & base : startingBases)
				{
					targetPos += base->getCenterOfBase();
				}
				targetPos /= static_cast<float>(startingBases.size());

				bPoint = fixpoint + Util::normalizeVector(targetPos - fixpoint, 5.0f);
			}
			else
			{
				bPoint = m_bot.Bases().getRallyPoint();
			}
		}
		return bPoint;
	}
}

std::queue<sc2::Point2D> MapTools::getEdgePath(const sc2::Point2D posStart, const sc2::Point2D posEnd) const
{
	// WAYPOINTS QUEUE
	std::queue<sc2::Point2D> wayPoints;
	const int margin = 5;
	const sc2::Point2D posA = m_bot.Map().getClosestBorderPoint(posStart, margin);
	sc2::Point2D posB = m_bot.Map().getClosestBorderPoint(posEnd, margin);
	const sc2::Point2D forbiddenCorner = m_bot.Map().getForbiddenCorner(margin);

	float x_min = m_bot.Observation()->GetGameInfo().playable_min.x + margin;
	float x_max = m_bot.Observation()->GetGameInfo().playable_max.x - margin;
	float y_min = m_bot.Observation()->GetGameInfo().playable_min.y + margin;
	float y_max = m_bot.Observation()->GetGameInfo().playable_max.y - margin;

	// we are at the same side
	if (posA.x == posB.x || posA.y == posB.y)
	{
		wayPoints.push(posA);
	}
	// other side
    else if ((posA.x == x_min && posB.x == x_max) || (posA.x == x_max && posB.x == x_min) || (posA.y == y_min && posB.y == y_max) || (posA.y == y_max && posB.y == y_min))
	{
		// Left to right
		if (posA.x == x_min)
		{
			wayPoints.push(sc2::Point2D(x_min, posStart.y));
			if (forbiddenCorner.y == y_max)
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
		}
		// Right to left
		else if (posA.x == x_max)
		{
			wayPoints.push(sc2::Point2D(x_max, posStart.y));
			if (forbiddenCorner.y == y_max)
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
		}
		// Down to up
		else if (posA.y == y_min)
		{
			wayPoints.push(sc2::Point2D(posStart.x, y_min));
			if (forbiddenCorner.x == x_max)
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
		}
		// Up down
		else if (posA.y == y_max)
		{
			wayPoints.push(sc2::Point2D(posStart.x, y_max));
			if (forbiddenCorner.x == x_max)
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
		}
	}
	else
	{
		wayPoints.push(posA);
		// Over an Edge
		// left to up
		if (posA.x == x_min && posB.y == y_max)
		{
			if (forbiddenCorner.x != x_min || forbiddenCorner.y != y_max)
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
		}
		// left to down
		else if (posA.x == x_min && posB.y == y_min)
		{
			if (forbiddenCorner.x != x_min || forbiddenCorner.y != y_min)
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
		}
		// right to up
		else if (posA.x == x_max && posB.y == y_max)
		{
			if (forbiddenCorner.x != x_max || forbiddenCorner.y != y_max)
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
		}
		// right to down
		else if (posA.x == x_max && posB.y == y_min)
		{
			if (forbiddenCorner.x != x_max || forbiddenCorner.y != y_min)
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
		}
		// down to left
		else if (posA.y == y_min && posB.x == x_min)
		{
			if (forbiddenCorner.x != x_min || forbiddenCorner.y != y_min)
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
		}
		// down to right
		else if (posA.y == y_min && posB.x == x_max)
		{
			if (forbiddenCorner.x != x_max || forbiddenCorner.y != y_min)
			{
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
		}
		// up to left
		else if (posA.y == y_max && posB.x == x_min)
		{
			if (forbiddenCorner.x != x_min || forbiddenCorner.y != y_max)
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
				wayPoints.push(sc2::Point2D(x_max, y_min));
				wayPoints.push(sc2::Point2D(x_min, y_min));
			}
		}
		// up to right
		else if (posA.y == y_max && posB.x == x_max)
		{
			if (forbiddenCorner.x != x_max || forbiddenCorner.y != y_max)
			{
				wayPoints.push(sc2::Point2D(x_max, y_max));
			}
			else
			{
				wayPoints.push(sc2::Point2D(x_min, y_max));
				wayPoints.push(sc2::Point2D(x_min, y_min));
				wayPoints.push(sc2::Point2D(x_max, y_min));
			}
		}
	}
	if (m_bot.Bases().isOccupiedBy(Players::Enemy, posEnd))
	{
		CUnits staticDs = m_bot.UnitInfo().getUnits(Players::Enemy, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET }));
		staticDs.erase(std::remove_if(staticDs.begin(), staticDs.end(), [posEnd](const auto & staticD) {
			return Util::DistSq(staticD->getPos(), posEnd) > 400.0f;
		}), staticDs.end());
		sc2::Point2D saveDirection;
		if (wayPoints.back() - posB != sc2::Point2D{ 0.0f, 0.0f })
		{
			saveDirection = Util::normalizeVector(wayPoints.back() - posB);
		}
		else
		{
			saveDirection = Util::normalizeVector(posB - forbiddenCorner);
		}
		bool inRange = true;
		while (inRange && isValid(posB))
		{
			inRange = false;
			while (calcThreatLvl(posB, sc2::Weapon::TargetType::Air) > 0)
			{
				posB += saveDirection;
				inRange = true;
			}
		}
		pathPlaning plan1(m_bot, wayPoints.back(), posB, m_bot.Map().width(), m_bot.Map().height(), 1.0f);
		for (const auto & wp : plan1.planPath())
		{
			wayPoints.push(wp);
		}
		pathPlaning plan2(m_bot, posB, posEnd, m_bot.Map().width(), m_bot.Map().height(), 1.0f);
		for (const auto & wp : plan2.planPath())
		{
			wayPoints.push(wp);
		}
	}
	else
	{
		wayPoints.push(posB);
		wayPoints.push(posEnd);
	}
	return wayPoints;
}

sc2::Point2D MapTools::findLandingZone(sc2::Point2D pos) const
{
	CUnits staticDs = m_bot.UnitInfo().getUnits(Players::Enemy, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON, sc2::UNIT_TYPEID::ZERG_SPORECRAWLER, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET }));
	staticDs.erase(std::remove_if(staticDs.begin(), staticDs.end(), [pos](const auto & staticD) {
		return Util::DistSq(staticD->getPos(), pos) > 400.0f;
	}), staticDs.end());

	const std::vector<sc2::Point2D> & tiles = getClosestTilesTo(pos);

	for (const auto & tile : tiles)
	{
		bool tooClose = false;
		for (const auto & staticD : staticDs)
		{
			const float range = staticD->getAttackRange();
			const float dist = Util::Dist(staticD->getPos(), tile);
			if (dist < 3.0f + range)
			{
				tooClose = true;
				break;
			}
		}
		if (!tooClose)
		{
			return tile;
		}
		if (Util::DistSq(pos, tile) > 400.0f)
		{
			break;
		}
	}
	return pos;
}

const sc2::Point2D MapTools::getForbiddenCorner(const int margin, const int player) const
{
	const BaseLocation * enemyBase = m_bot.Bases().getPlayerStartingBaseLocation(player);
	if (!enemyBase)
	{
		return sc2::Point2D(-1.0f, -1.0f);
	}
	const sc2::Point2D pos = enemyBase->getCenterOfBase();

	const float x_min = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_min.x + margin);
	const float x_max = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_max.x - margin);
	const float y_min = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_min.y + margin);
	const float y_max = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_max.y - margin);
	if (pos.x - x_min < x_max - pos.x)
	{
		if (pos.y - y_min < y_max - pos.y)
		{
			return sc2::Point2D(x_min, y_min);
		}
		else
		{
			return sc2::Point2D(x_min, y_max);
		}
	}
	else
	{
		if (pos.y - y_min < y_max - pos.y)
		{
			return sc2::Point2D(x_max, y_min);
		}
		else
		{
			return sc2::Point2D(x_max, y_max);
		}
	}
}

const sc2::Point2D MapTools::getClosestBorderPoint(sc2::Point2D pos, int margin) const
{
	const float x_min = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_min.x + margin);
	const float x_max = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_max.x - margin);
	const float y_min = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_min.y + margin);
	const float y_max = static_cast<float>(m_bot.Observation()->GetGameInfo().playable_max.y - margin);
	if (pos.x - x_min < x_max - pos.x)
	{
		if (pos.y - y_min < y_max - pos.y)
		{
			if (pos.x - x_min < pos.y - y_min)
			{
				return sc2::Point2D(x_min, pos.y);
			}
			else
			{
				return sc2::Point2D(pos.x, y_min);
			}
		}
		else
		{
			if (pos.x - x_min < y_max - pos.y)
			{
				return sc2::Point2D(x_min, pos.y);
			}
			else
			{
				return sc2::Point2D(pos.x, y_max);
			}
		}
	}
	else
	{
		if (pos.y - y_min < y_max - pos.y)
		{
			if (x_max - pos.x < pos.y - y_min)
			{
				return sc2::Point2D(x_max, pos.y);
			}
			else
			{
				return sc2::Point2D(pos.x, y_min);
			}
		}
		else
		{
			if (x_max - pos.x < y_max-pos.y)
			{
				return sc2::Point2D(x_max, pos.y);
			}
			else
			{
				return sc2::Point2D(pos.x, y_max);
			}
		}
	}
}

bool MapTools::hasPocketBase() const
{
	const BaseLocation * homeBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
	const BaseLocation * firstExe = nullptr;
	int minDistance = std::numeric_limits<int>::max();
	for (const auto & base : m_bot.Bases().getBaseLocations())
	{
		if (homeBase->getBaseID() == base->getBaseID())
		{
			continue;
		}
		auto tile = base->getCenterOfBase();
		int distanceFromHome = homeBase->getGroundDistance(tile);
		if (distanceFromHome <= 0)
		{
			continue;
		}
		if (!firstExe || distanceFromHome < minDistance)
		{
			firstExe = base;
			minDistance = distanceFromHome;
		}
	}
	// Any enemy base is fine
	const sc2::Point2D enemyStartBase = m_bot.Observation()->GetGameInfo().enemy_start_locations.front();
	return firstExe ? homeBase->getGroundDistance(enemyStartBase) <= firstExe->getGroundDistance(enemyStartBase) : false;
}

const CUnit_ptr MapTools::workerSlideMineral(const sc2::Point2D workerPos, const sc2::Point2D enemyPos) const
{
	const CUnits minerals = m_bot.UnitInfo().getUnits(Players::Neutral, Util::getMineralTypes());
	CUnit_ptr slideMineral = nullptr;
	float closestDistance = std::numeric_limits<float>::max();
	for (const auto & mineral : minerals)
	{
		const float distWorker = Util::Dist(workerPos, mineral->getPos());
		const float distEnemy = Util::Dist(enemyPos, mineral->getPos());
		if (distEnemy < distWorker && closestDistance > distEnemy)
		{
			closestDistance = distEnemy;
			slideMineral = mineral;
		}
	}
	return slideMineral;
}

float MapTools::getHeight(const sc2::Point2D pos) const
{
	return m_bot.Observation()->TerrainHeight(pos);
}
float MapTools::getHeight(const float x, const float y) const
{
	return m_bot.Observation()->TerrainHeight(sc2::Point2D(x, y));
}

void MapTools::draw() const
{
	sc2::Point2D camera = m_bot.Observation()->GetCameraPos();
	for (float x = camera.x - 16.0f; x < camera.x + 16.0f; ++x)
	{
		for (float y = camera.y - 16.0f; y < camera.y + 16.0f; ++y)
		{
			if (!isValid(static_cast<int>(x), static_cast<int>(y)))
			{
				continue;
			}

			if (m_bot.Config().DrawWalkableSectors)
			{
				std::stringstream ss;
				ss << getSectorNumber(static_cast<int>(x), static_cast<int>(y));
				Drawing::drawTextScreen(m_bot, sc2::Point3D(x + 0.5f, y + 0.5f, m_maxZ + 0.1f), ss.str(), sc2::Colors::Yellow);
			}

			if (m_bot.Config().DrawTileInfo)
			{
				sc2::Color color = isWalkable(static_cast<int>(x), static_cast<int>(y)) ? sc2::Colors::Green : sc2::Colors::Red;
				if (isWalkable(static_cast<int>(x), static_cast<int>(y)) && !isBuildable(static_cast<int>(x), static_cast<int>(y)))
				{
					color = sc2::Colors::Yellow;
				}

				Drawing::drawSquare(m_bot, x, y, x + 1, y + 1, color);
			}
		}
	}
}

float MapTools::calcThreatLvl(sc2::Point2D pos, const sc2::Weapon::TargetType & targetType) const
{
	float threatLvl = 0.0f;
	for (const auto & enemy : m_bot.UnitInfo().getUnits(Players::Enemy))
	{
		// if it's a combat unit
		if (enemy->isCombatUnit())
		{
			// If we are in range.
			float sightRange = enemy->getSightRange();
			float attackRange = enemy->getAttackRange(targetType);
			float dist = Util::Dist(enemy->getPos(), pos);
			if (dist < sightRange)
			{
				// Get its weapon
				const sc2::Weapon weapon = enemy->getWeapon(targetType);
				// I ignore bonus dmg for now.
				float dps = weapon.attacks * weapon.damage_ / weapon.speed;
				threatLvl += std::max(0.0f, dps*(sightRange - dist) / sightRange);
				if (dist < attackRange)
				{
					threatLvl += 10;
				}
			}
		}
	}
	return threatLvl;
}

/*
void MapTools::printMap() const
{
	std::stringstream ss;
	for (int y(0); y < m_height; ++y)
	{
		for (int x(0); x < m_width; ++x)
		{
			ss << isWalkable(x, y);
		}

		ss << "\n";
	}

	std::ofstream out("map.txt");
	out << ss.str();
	out.close();
}
*/
