#pragma once

#include "Common.h"
#include <queue>

class CCBot;

class ScoutManager
{
    CCBot &   m_bot;

    const sc2::Unit * m_scoutUnit;
    std::string     m_scoutStatus;
    int             m_numScouts;
    bool            m_scoutUnderAttack;
    float           m_previousScoutHP;
	std::queue<sc2::Point2D> m_targetBasesPositions;
	bool			m_foundProxy;

    bool            enemyWorkerInRadiusOf(const sc2::Point2D & pos) const;
    sc2::Point2D    getFleePosition() const;
    const sc2::Unit * closestEnemyWorkerTo(const sc2::Point2D & pos) const;
	const sc2::Unit * closestEnemyCombatTo(const sc2::Point2D & pos) const;
	std::vector<const sc2::Unit*> getEnemyUnitsInSight(const sc2::Point2D & pos) const;
	bool enemyTooClose(std::vector<const sc2::Unit*> enemyUnitsInSight);
	bool attackEnemyCombat(std::vector<const sc2::Unit*> enemyUnitsInSight);
	bool attackEnemyWorker(std::vector<const sc2::Unit*> enemyUnitsInSight);
    void            moveScouts();
    void            drawScoutInformation();
	void			scoutDamaged();


public:

    ScoutManager(CCBot & bot);

    void onStart();
    void onFrame();
	void checkOurBases();
    void setWorkerScout(const sc2::Unit * unit);
	void setScout(const sc2::Unit * unit);
	int getNumScouts();
	void updateNearestUnoccupiedBases(sc2::Point2D pos,int player);
	const bool dontBlowYourselfUp() const;
	void scoutRequested();
	void raiseAlarm(std::vector<const sc2::Unit *> enemyUnitsInSight);
};