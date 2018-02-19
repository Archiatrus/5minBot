#pragma once

#include "CUnit.h"
#include "Common.h"
#include <queue>

class CCBot;

class ScoutManager
{
    CCBot &   m_bot;

	CUnit_ptr m_scoutUnit;
    std::string     m_scoutStatus;
    int             m_numScouts;
    bool            m_scoutUnderAttack;
    float           m_previousScoutHP;
	std::queue<sc2::Point2D> m_targetBasesPositions;
	bool			m_foundProxy;

    bool            enemyWorkerInRadiusOf(const sc2::Point2D & pos) const;
    sc2::Point2D    getFleePosition() const;
    CUnit_ptr closestEnemyWorkerTo(const sc2::Point2D & pos) const;
	CUnit_ptr closestEnemyCombatTo(const sc2::Point2D & pos) const;
	bool enemyTooClose(CUnits enemyUnitsInSight);
	bool attackEnemyCombat(CUnits enemyUnitsInSight);
	bool attackEnemyWorker(CUnits enemyUnitsInSight);
    void            moveScouts();
    void            drawScoutInformation();
	void			scoutDamaged();


public:

    ScoutManager(CCBot & bot);

    void onStart();
    void onFrame();
	void checkOurBases();
    void setWorkerScout(CUnit_ptr unit);
	void setScout(CUnit_ptr unit);
	int getNumScouts();
	void updateNearestUnoccupiedBases(sc2::Point2D pos,int player);
	const bool dontBlowYourselfUp() const;
	void scoutRequested();
	void raiseAlarm(CUnits enemyUnitsInSight);
};