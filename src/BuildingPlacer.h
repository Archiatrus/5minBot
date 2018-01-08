#pragma once

#include "Common.h"
#include "BuildingData.h"

class CCBot;
class BaseLocation;

struct buildingPlace
{
	const sc2::Point2D m_seed;
	const int m_footPrintArea;
	const Building m_building;
	const std::vector<sc2::Point2D> m_closestTiles;
	int m_idx;
	bool m_canBuildHere;

	buildingPlace(const sc2::Point2D seed, const int footPrintArea, const Building building, const std::vector<sc2::Point2D> closestTiles) :m_seed(seed), m_footPrintArea(footPrintArea), m_building(building), m_closestTiles(closestTiles), m_idx(0), m_canBuildHere(false)
	{
	}

	buildingPlace(const sc2::Point2D seed, const int footPrintArea) :m_seed(seed), m_footPrintArea(footPrintArea)
	{
	}

	const bool operator==(const buildingPlace & rhs) const
	{
		if (this->m_seed.x == rhs.m_seed.x
			&& this->m_seed.y == rhs.m_seed.y
			&& this->m_footPrintArea == rhs.m_footPrintArea)
		{
			return true;
		}
		return false;
	}
};

class BuildingPlacer
{
    CCBot & m_bot;

    std::vector< std::vector<bool> > m_reserveMap;
	std::vector<buildingPlace> m_buildLocationTester;

	void expandBuildingTesterOnce();
    // queries for various BuildingPlacer data
    bool			buildable(const Building & b, int x, int y) const;
    bool			isReserved(int x, int y) const;
    bool			isInResourceBox(int x, int y) const;
    bool			tileOverlapsBaseLocation(int x, int y, sc2::UnitTypeID type) const;


public:

    BuildingPlacer(CCBot & bot);

    void onStart();

	void onFrame();

    // determines whether we can build at a given location
    bool			canBuildHere(int bx, int by, const Building & b) const;
    bool			canBuildHereWithSpace(int bx, int by, const Building & b, int buildDist) const;

    // returns a build location near a building's desired location
    sc2::Point2D	getBuildLocationNear(const Building & b, int buildDist);
	sc2::Point2D	getTownHallLocationNear(const Building & b);

    void			drawReservedTiles();

    void			reserveTiles(int x, int y, int width, int height);
    void			freeTiles(int x, int y, int width, int height);
	void			freeTiles();
    sc2::Point2D	getRefineryPosition();
};

extern bool useDebug;