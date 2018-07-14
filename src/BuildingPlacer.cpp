#include "Common.h"
#include "BuildingPlacer.h"
#include "CCBot.h"
#include "Building.h"
#include "Util.h"
#include "Drawing.h"

BuildingPlacer::BuildingPlacer(CCBot & bot)
	: m_bot(bot)
{
}

void BuildingPlacer::onStart()
{
	m_reserveMap = std::vector< std::vector<bool> >(m_bot.Map().width(), std::vector<bool>(m_bot.Map().height(), false));

	sc2::Point2D buildingSeedPosition = m_bot.Bases().getBuildingLocation();
	const std::vector<sc2::Point2D> & closestToBuilding = m_bot.Map().getClosestTilesTo(buildingSeedPosition);
	buildingPlace depots(buildingSeedPosition, 4, Building(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, sc2::Point2D(), 2), closestToBuilding);
	buildingPlace production(buildingSeedPosition, 9, Building(sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::Point2D(), 3), closestToBuilding);
	m_buildLocationTester.push_back(depots);
	m_buildLocationTester.push_back(production);
}

void BuildingPlacer::onFrame()
{
	expandBuildingTesterOnce();
}

void BuildingPlacer::expandBuildingTesterOnce()
{
	for (auto & buildLocationTester : m_buildLocationTester)
	{
		if (useDebug && buildLocationTester.m_canBuildHere && buildLocationTester.m_footPrintArea != 25)
		{
			auto newPos = buildLocationTester.m_closestTiles[buildLocationTester.m_idx];
			Drawing::drawSphere(m_bot, newPos, sqrtf(static_cast<float>(buildLocationTester.m_footPrintArea))/2.0f, sc2::Colors::Yellow);
		}
		// We don't need to find another base right next to the old one
		if (buildLocationTester.m_canBuildHere || buildLocationTester.m_footPrintArea == 25 || buildLocationTester.m_closestTiles.size() >= buildLocationTester.m_idx)
		{
			continue;
		}
		sc2::Point2D pos = buildLocationTester.m_closestTiles[buildLocationTester.m_idx];

		if (canBuildHereWithSpace(pos.x, pos.y, buildLocationTester.m_building, m_bot.Config().BuildingSpacing))
		{
			buildLocationTester.m_canBuildHere = true;
		}
		else
		{
			++buildLocationTester.m_idx;
		}
		break;
	}
}

bool BuildingPlacer::isInResourceBox(float x, float y) const
{
	return false;
	// return m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->isInResourceBox(x, y);
}

// makes final checks to see if a building can be built at a certain location
bool BuildingPlacer::canBuildHere(float bx, float by, const Building & b) const
{
	if (isInResourceBox(by, by))
	{
		return false;
	}

	// if it overlaps a base location return false
	if (tileOverlapsBaseLocation(bx, by, b.m_type))
	{
		return false;
	}
	// Don't build below flying buildings
	const CUnits flyingBuildings = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING }));
	sc2::Point2D loc(static_cast<float>(bx), static_cast<float>(by));
	for (const auto & unit : flyingBuildings)
	{
		if (Util::Dist(loc, unit->getPos()) < 3)
		{
			return false;
		}
	}
	return true;
}

// returns true if we can build this type of unit here with the specified amount of space.
bool BuildingPlacer::canBuildHereWithSpace(float bx, float by, const Building & b, int buildDist) const
{
	
	sc2::UnitTypeID type = b.m_type;
	// if we can't build here, we of course can't build here with space
	if (!canBuildHere(bx, by, b))
	{
		return false;
	}
	if (Util::IsTownHallType(b.m_type))
	{
		return buildable(b, bx, by);
	}

	// height and width of the building
	int width  = Util::GetUnitTypeWidth(b.m_type, m_bot);
	int height = Util::GetUnitTypeHeight(b.m_type, m_bot);

	// define the rectangle of the building spot
	float startx = bx - (width / 2.0 + buildDist);
	float starty = by - (height / 2.0 + buildDist);
	float endx   = bx + (width / 2.0 + buildDist);
	float endy   = by + (height / 2.0 + buildDist);

	if (b.m_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS || b.m_type == sc2::UNIT_TYPEID::TERRAN_FACTORY || b.m_type == sc2::UNIT_TYPEID::TERRAN_STARPORT)
	{
		for (const auto & mineral : m_bot.UnitInfo().getUnits(Players::Neutral, Util::getMineralTypes()))
		{
			if (Util::DistSq(sc2::Point2D(bx, by), mineral->getPos()) < 16.0f)
			{
				return false;
			}
		}
		for (const auto & mineral : m_bot.UnitInfo().getUnits(Players::Neutral, Util::getGeyserTypes()))
		{
			if (Util::DistSq(sc2::Point2D(bx, by), mineral->getPos()) < 16.0f)
			{
				return false;
			}
		}
		--startx;
		endx += 2;
	}
	// if this rectangle doesn't fit on the map we can't build here
	if (startx < 0 || starty < 0 || endx > m_bot.Map().width() || endy > m_bot.Map().height())
	{
		return false;
	}
	// if we can't build here, or space is reserved, or it's in the resource box, we can't build here
	std::vector<sc2::QueryInterface::PlacementQuery> query;
	if (!Util::IsRefineryType(b.m_type) && !Util::IsTownHallType(b.m_type))
	{
		for (int x = startx; x < endx; x++)
		{
			for (int y = starty; y < endy; y++)
			{
				if (m_reserveMap[x][y])  // || !buildable(b, x, y))
				{
					return false;
				}
				query.push_back({ sc2::ABILITY_ID::BUILD_SENSORTOWER, sc2::Point2D(x + 0.5f, y + 0.5f) });
			}
		}
	}
	std::vector<bool> results = m_bot.Query()->Placement(query);
	if (std::find(results.begin(), results.end(), false) != results.end())
	{
		return false;
	}
	return buildable(b, bx, by);
}

// We will never speak of this again....
sc2::Point2D BuildingPlacer::getTownHallLocationNear(const Building & b)
{
	return getBuildLocationNear(b, 0);
}

sc2::Point2D BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist)
{
	// Timer t;
	// t.start();

	// get the precomputed vector of tile positions which are sorted closes to this location

	const int buildingHash = Util::GetUnitTypeWidth(b.m_type, m_bot)*Util::GetUnitTypeHeight(b.m_type, m_bot);
	buildingPlace newBuildingPlacePrototype(b.desiredPosition, buildingHash);
	auto idx = std::find(m_buildLocationTester.begin(), m_buildLocationTester.end(), newBuildingPlacePrototype);
	if (idx == m_buildLocationTester.end())
	{
		const std::vector<sc2::Point2D> & closestToBuilding = m_bot.Map().getClosestTilesTo(b.desiredPosition);
		if (closestToBuilding.size() > 1)
		{
			buildingPlace newBuildingPlace(b.desiredPosition, buildingHash, b, closestToBuilding);
			m_buildLocationTester.push_back(newBuildingPlace);
			idx = std::prev(m_buildLocationTester.end());
		}
		else
		{
			const std::vector<sc2::Point2D> & closestToBuilding2 = m_bot.Map().getClosestTilesToAir(b.desiredPosition);
			buildingPlace newBuildingPlace(b.desiredPosition, buildingHash, b, closestToBuilding2);
			m_buildLocationTester.push_back(newBuildingPlace);
			idx = std::prev(m_buildLocationTester.end());
		}
	}
	// iterate through the list until we've found a suitable location
	for (int i = idx->m_idx; i != idx->m_closestTiles.size(); ++i)
	{
		sc2::Point2D pos = idx->m_closestTiles[i];
		// Drawing::drawSphere(m_bot, pos, 1.0f);
		if (canBuildHereWithSpace(pos.x, pos.y, b, buildDist))
		{
			// double ms = t.getElapsedTimeInMilliSec();
			// printf("Building Placer Took %d iterations, lasting %lf ms @ %lf iterations/ms, %lf setup ms\n", (int)i, ms, (i / ms), ms1);
			idx->m_idx = i;
			// We are building a building. So all building places are invalid
			for (auto & buildPlace : m_buildLocationTester)
			{
				buildPlace.m_canBuildHere = false;
			}
			return pos;
		}
	}

	// double ms = t.getElapsedTimeInMilliSec();
	// printf("Building Placer Took %lf ms\n", ms);

	return sc2::Point2D(0, 0);
}

bool BuildingPlacer::tileOverlapsBaseLocation(int x, int y, sc2::UnitTypeID type) const
{
	// if it's a resource depot we don't care if it overlaps
	if (Util::IsTownHallType(type))
	{
		return false;
	}

	// dimensions of the proposed location
	int tx1 = x;
	int ty1 = y;
	int tx2 = tx1 + Util::GetUnitTypeWidth(type, m_bot);
	int ty2 = ty1 + Util::GetUnitTypeHeight(type, m_bot);
	if (type == sc2::UNIT_TYPEID::TERRAN_BARRACKS)
	{
		tx2 += 2;
	}
	// for each base location
	for (const BaseLocation * base : m_bot.Bases().getBaseLocations())
	{
		// dimensions of the base location
		int bx1 = static_cast<int>(base->getCenterOfBase().x);
		int by1 = static_cast<int>(base->getCenterOfBase().y);
		int bx2 = bx1 + Util::GetUnitTypeWidth(Util::GetTownHall(m_bot.GetPlayerRace(Players::Self)), m_bot);
		int by2 = by1 + Util::GetUnitTypeHeight(Util::GetTownHall(m_bot.GetPlayerRace(Players::Self)), m_bot);

		// conditions for non-overlap are easy
		bool noOverlap = (tx2 < bx1) || (tx1 > bx2) || (ty2 < by1) || (ty1 > by2);

		// if the reverse is true, return true
		if (!noOverlap)
		{
			return true;
		}
	}

	// otherwise there is no overlap
	return false;
}

bool BuildingPlacer::buildable(const Building & b, float x, float y) const
{
	if (!m_bot.Map().isValid(static_cast<int>(x), static_cast<int>(y)) || !m_bot.Map().canBuildTypeAtPosition(x, y, b.m_type))
	{
		return false;
	}
	return true;
}

void BuildingPlacer::reserveTiles(float bx, float by, float width, float height, bool addon)
{
	size_t rwidth = m_reserveMap.size();
	size_t rheight = m_reserveMap[0].size();
	for (size_t x = static_cast<size_t>(bx - width/2.0f); x < static_cast<size_t>(bx + width/2.0f + addon*2.0f) && x < rwidth; x++)
	{
		for (size_t y = static_cast<size_t>(by - height/2.0f); y < static_cast<size_t>(by + height/2.0f) && y < rheight; y++)
		{
			m_reserveMap[x][y] = true;
		}
	}
}

void BuildingPlacer::drawReservedTiles()
{
	if (false)//!m_bot.Config().DrawReservedBuildingTiles)
	{
		return;
	}

	int rwidth = static_cast<int>(m_reserveMap.size());
	int rheight = static_cast<int>(m_reserveMap[0].size());

	for (int x = 0; x < rwidth; ++x)
	{
		for (int y = 0; y < rheight; ++y)
		{
			if (m_reserveMap[x][y] || isInResourceBox(x, y))
			{
				Drawing::drawSphere(m_bot, sc2::Point2D(x + 0.5f, y + 0.5f), 0.5f, sc2::Colors::Yellow);
			}
		}
	}
}

void BuildingPlacer::freeTiles(float bx, float by, float width, float height)
{
	int rwidth = static_cast<int>(m_reserveMap.size());
	int rheight = static_cast<int>(m_reserveMap[0].size());

	for (size_t x = static_cast<size_t>(bx - width / 2.0f); x < static_cast<size_t>(bx + width / 2.0f) && x < rwidth; x++)
	{
		for (size_t y = static_cast<size_t>(by - height / 2.0f); y < static_cast<size_t>(by + height / 2.0f) && y < rheight; y++)
		{
			m_reserveMap[x][y] = false;
		}
	}
}

void BuildingPlacer::freeTiles()
{
	m_reserveMap = std::vector< std::vector<bool> >(m_bot.Map().width(), std::vector<bool>(m_bot.Map().height(), false));
}

sc2::Point2D BuildingPlacer::getRefineryPosition()
{
	sc2::Point2D closestGeyser(0, 0);
	double minGeyserDistanceFromHome = std::numeric_limits<double>::max();
	sc2::Point2D homePosition = m_bot.GetStartLocation();

	for (const auto & geyser : m_bot.UnitInfo().getUnits(Players::Neutral, Util::getGeyserTypes()))
	{
		sc2::Point2D geyserPos(geyser->getPos());

		// check to see if it's next to one of our depots
		bool nearDepot = false;
		for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes()))
		{
			if (Util::Dist(unit->getPos(), geyserPos) < 10)
			{
				nearDepot = true;
			}
		}
		if (nearDepot)
		{
			double homeDistance = Util::Dist(geyser->getPos(), homePosition);

			if (m_bot.Query()->Placement(sc2::ABILITY_ID::BUILD_REFINERY, geyserPos))
			{
				if (homeDistance < minGeyserDistanceFromHome)
				{
					minGeyserDistanceFromHome = homeDistance;
					closestGeyser = geyser->getPos();
				}
			}
		}
	}

	return closestGeyser;
}

bool BuildingPlacer::isReserved(int x, int y) const
{
	int rwidth = static_cast<int>(m_reserveMap.size());
	int rheight = static_cast<int>(m_reserveMap[0].size());
	if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
	{
		return false;
	}

	return m_reserveMap[x][y];
}
