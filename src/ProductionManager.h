#pragma once
#include "sc2api/sc2_api.h"
#include "Common.h"
#include "BuildOrder.h"
#include "BuildingManager.h"
#include "BuildOrderQueue.h"

class CCBot;

class ProductionManager
{
	CCBot &	   m_bot;

	BuildingManager m_buildingManager;
	std::deque<BuildOrderItem> m_newQueue;

	bool m_scoutRequested;
	bool m_vikingRequested;
	bool m_liberatorRequested;
	int m_scansRequested;
	int m_defaultMacroSleep;
	const int m_defaultMacroSleepMax;
	bool m_turretsRequested;
	bool m_needCC;

	bool    detectBuildOrderDeadlock();
	void    create(BuildOrderItem item);
	void    manageBuildOrderQueue();
	int     getFreeMinerals();
	int     getFreeGas();

	

public:

	ProductionManager(CCBot & bot);

	void    onStart();
	void    onFrame();
	void    onUnitDestroy(CUnit_ptr unit);
	void    drawProductionInformation();

	void defaultMacro();

	void requestScout();
	void requestVikings();
	void requestTurrets();
	void requestLiberator();
	void requestScan();
	void usedScan(const int i=1);
	int buildingsFinished(const CUnits units) const;
	void needCC();
	int howOftenQueued(sc2::UnitTypeID type);
};
