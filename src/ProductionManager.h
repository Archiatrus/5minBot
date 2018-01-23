#pragma once
#include "sc2api/sc2_api.h"
#include "Common.h"
#include "BuildOrder.h"
#include "BuildingManager.h"
#include "BuildOrderQueue.h"

class CCBot;

class ProductionManager
{
    CCBot &       m_bot;

    BuildingManager m_buildingManager;
	std::deque<BuildOrderItem> m_newQueue;

	int m_weapons;
	int m_armor;

	bool m_scoutRequested;
	bool m_vikingRequested;
	int m_scansRequested;

    //const sc2::Unit * getClosestUnitToPosition(const std::vector<const sc2::Unit *> & units, sc2::Point2D closestTo);
    //bool    meetsReservedResources(const BuildType & type);
    //bool    canMakeNow(const sc2::Unit * producer, const BuildType & type);
    bool    detectBuildOrderDeadlock();
    //void    setBuildOrder(const BuildOrder & buildOrder);
    void    create(BuildOrderItem item);
    void    manageBuildOrderQueue();
    int     getFreeMinerals();
    int     getFreeGas();

	

public:

    ProductionManager(CCBot & bot);

    void    onStart();
    void    onFrame();
    void    onUnitDestroy(const sc2::Unit * unit);
    void    drawProductionInformation();

	void defaultMacro();

    //const sc2::Unit * getProducer(const BuildType & type, sc2::Point2D closestTo = sc2::Point2D(0, 0));

	void requestScout();
	void requestVikings();
	void requestScan();
	void usedScan(const int i=1);
	int buildingsFinished(sc2::Units units);
	int howOftenQueued(sc2::UnitTypeID type);
};
