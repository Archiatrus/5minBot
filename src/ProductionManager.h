#pragma once
#include "sc2api/sc2_api.h"
#include "Common.h"
#include "BuildOrder.h"
#include "BuildingManager.h"
#include "BuildOrderQueue.h"

class CCBot;

enum class ProductionStatus{notEnoughSupply, notEnoughMinerals, notEnoughGas, noIdleProduction, success };

class ProductionManager
{
private:
    CCBot &	   m_bot;

	BuildingManager m_buildingManager;
	std::deque<BuildOrderItem> m_newQueue;

	bool m_scoutRequested;
	bool m_vikingRequested;
	bool m_liberatorRequested;
    size_t m_scansRequested;
    size_t m_defaultMacroSleep;
    const size_t m_defaultMacroSleepMax;
	bool m_turretsRequested;
	bool m_needCC;

	bool    detectBuildOrderDeadlock();
	void    create(BuildOrderItem item);
	void    manageBuildOrderQueue();
    size_t     getFreeMinerals();
    size_t     getFreeGas();
    size_t getSuggestedNumTechLabs() const;
	

public:

	ProductionManager(CCBot & bot);

	void    onStart();
	void    onFrame();
	void    onUnitDestroy(const sc2::Unit * unit);
	void    drawProductionInformation();

    void defaultMacroBio();
    void macroMechanic();
    ProductionStatus pleaseTrain(const sc2::UNIT_TYPEID & unitType, const size_t minerals, const size_t gas);

	void requestScout();
	void requestVikings();
	void requestTurrets();
	void requestLiberator();
	void requestScan();
    void usedScan(const size_t i=1);
    size_t buildingsFinished(const CUnits units) const;
	void needCC();
    size_t howOftenQueued(sc2::UnitTypeID type);
    bool tryingToExpand() const;
};
