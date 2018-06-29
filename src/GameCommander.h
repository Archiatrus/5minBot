#pragma once

#include "Common.h"
#include "Timer.hpp"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "HarassManager.h"
#include "CombatCommander.h"
#include "ShuttleService.h"

class CCBot;

struct timePlace
{
	uint32_t m_time;
	sc2::Point2D m_place;

	timePlace(uint32_t time=0, sc2::Point2D place= sc2::Point2D(0,0));
};

class GameCommander
{
	CCBot &				 m_bot;
	Timer				   m_timer;

	ProductionManager	   m_productionManager;
	ScoutManager			m_scoutManager;
	HarassManager			m_harassManager;
	CombatCommander		 m_combatCommander;

	CUnits    m_validUnits;
	CUnits    m_combatUnits;
	std::vector<std::shared_ptr<shuttle>>	m_shuttles;

	std::vector<timePlace> m_needDetections;

	bool					m_initialScoutSet;

	void assignUnit(CUnit_ptr, CUnits & units);
	bool isAssigned(CUnit_ptr unit) const;

	bool isShuttle(CUnit_ptr unit) const;
	bool isScout(CUnit_ptr unit) const;
	bool isHarassUnit(CUnit_ptr unit) const;

	void handleShuttleService();

public:

	GameCommander(CCBot & bot);

	void onStart();
	void onFrame();

	void handleUnitAssignments();
	void setValidUnits();
	void setShuttles();
	void setScoutUnits();
	void setHarassUnits();
	void setCombatUnits();


	void drawDebugInterface();
	void drawGameInformation(const sc2::Point2D pos);

	bool shouldSendInitialScout();


	void onUnitCreate(CUnit_ptr unit);
	void OnBuildingConstructionComplete(CUnit_ptr unit);
	void onUnitDestroy(CUnit_ptr unit);
	void OnUnitEnterVision(CUnit_ptr unit);
	void OnDetectedNeeded(const sc2::UnitTypeID & type, const sc2::Point2D & pos);

	ProductionManager & Production();
	void handleScans();
	void requestGuards(const bool req);
	std::shared_ptr<shuttle> requestShuttleService(CUnits passengers, const sc2::Point2D targetPos);
	const bool underAttack() const;
	void attack(const bool attack);
};
