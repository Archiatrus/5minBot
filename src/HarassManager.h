#pragma once

#include "Common.h"
#include "BaseLocation.h"
#include "ShuttleService.h"
#include "HarassLiberator.h"
#include <queue>
class CCBot;


namespace HarassStatus
{
	enum { Idle, Loading, OnMyWay, Harass, Fleeing, Refueling, GoingHome, Doomed };
}

namespace WMStatus
{
	enum { NewWM, WaitingForShuttle, ShuttleTransport, Harass, Relocating, Dead};
}

class Hitsquad
{
	CCBot &   m_bot;

	int	m_status;
	CUnit_ptr m_medivac;
	CUnits	m_marines;
	CUnits	m_doomedMarines;
	std::queue<sc2::Point2D> m_wayPoints;
	int m_pathPlanCounter;
	const BaseLocation *m_target;
	sc2::Point2D m_stalemateCheck;
	int32_t m_lastPathPlan;

	void checkForCasualties();
	CUnit_ptr getTargetMarines(CUnits targets) const;
	void stalemateCheck();
    bool manhattenMove(const BaseLocation * target);
	CUnits getNearbyEnemyUnits() const;
	const BaseLocation * getSavePosition() const;
    bool shouldWeFlee(CUnits targets, int threshold) const;
	void escapePathPlaning();
    bool haveNoTarget() const;

 public:
	Hitsquad(CCBot & bot, CUnit_ptr medivac);
	~Hitsquad();

    bool	addMedivac(CUnit_ptr medivac);
    bool	addMarine(CUnit_ptr marine);
    int	getNumMarines() const;
	CUnits	getMarines() const;
	CUnit_ptr getMedivac() const;
    int getStatus() const;
	bool harass(const BaseLocation *pos = nullptr);
};

class WMHarass
{
	CCBot &   m_bot;
	CUnit_ptr m_widowmine;
	uint32_t m_lastLoopEnemySeen;
	int m_status;
	std::shared_ptr<shuttle> m_shuttle;

	std::queue<sc2::Point2D> m_wayPoints;
	void getWayPoints(const sc2::Point2D targetPos);
	void replanWayPoints(const sc2::Point2D targetPos);
 public:
	WMHarass(CCBot & bot);


    bool	addWidowMine(CUnit_ptr widowMine);
	CUnit_ptr getwidowMine() const;
	void harass(const sc2::Point2D pos);
};

class HarassManager
{
	CCBot &   m_bot;

	std::map<bool, Hitsquad>	m_hitSquads;
	HarassLiberator m_liberatorHarass;
	WMHarass m_WMHarass;
	int	m_numHitSquads = 1;

	std::vector<const BaseLocation *> getHitSquadTargets() const;
	// Medivac hit squad
	void handleHitSquads();
	// Liberator harass
	void handleLiberator();
	const BaseLocation * getLiberatorTarget();
	// Widow mine harass
	void handleWMHarass();

 public:
	HarassManager(CCBot & bot);

	void onStart();
	void onFrame();

    bool needMedivac() const;
    bool needMarine() const;
    bool needLiberator();
    bool needWidowMine() const;

    bool setMedivac(CUnit_ptr medivac);
    bool setMarine(CUnit_ptr marine);
    bool setLiberator(const CUnit_ptr liberator);
    bool setWidowMine(CUnit_ptr widowMine);

	CUnits getMedivacs();
	CUnits getMarines();
	CUnit_ptr getWidowMine();
};
