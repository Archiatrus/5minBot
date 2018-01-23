#pragma once

#include "Common.h"
#include "Timer.hpp"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "HarassManager.h"
#include "CombatCommander.h"

class CCBot;

struct timePlace
{
	uint32_t m_time;
	sc2::Point2D m_place;

	timePlace(uint32_t time=0, sc2::Point2D place= sc2::Point2D(0,0));
};

class GameCommander
{
    CCBot &                 m_bot;
    Timer                   m_timer;

    ProductionManager       m_productionManager;
    ScoutManager            m_scoutManager;
	HarassManager			m_harassManager;
    CombatCommander         m_combatCommander;

    std::vector<const sc2::Unit *>    m_validUnits;
    std::vector<const sc2::Unit *>    m_combatUnits;
	std::vector<const sc2::Unit *>    m_scoutUnits;
	std::vector<const sc2::Unit *>    m_harassUnits;

	std::vector<timePlace> m_DTdetections;

    bool                    m_initialScoutSet;

    void assignUnit(const sc2::Unit * unit, std::vector<const sc2::Unit *> & units);
    bool isAssigned(const sc2::Unit * unit) const;

public:

    GameCommander(CCBot & bot);

    void onStart();
    void onFrame();

    void handleUnitAssignments();
    void setValidUnits();
    void setScoutUnits();
	void setHarassUnits();
    void setCombatUnits();


    void drawDebugInterface();
    void drawGameInformation(int x, int y);

    bool shouldSendInitialScout();


    void onUnitCreate(const sc2::Unit * unit);
	void OnBuildingConstructionComplete(const sc2::Unit * unit);
    void onUnitDestroy(const sc2::Unit * unit);
	void OnUnitEnterVision(const sc2::Unit * unit);
	void OnDTdetected(const sc2::Point2D pos);

	const ProductionManager & Production() const;
	void handleDTdetections();
};
