#pragma once

#include "Common.h"
#include "BaseLocation.h"
#include "ShuttleService.h"
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

	void checkForCasualties();
	CUnit_ptr getTargetMarines(CUnits targets) const;
	const bool manhattenMove(const BaseLocation * target);
	CUnits getNearbyEnemyUnits() const;
	const BaseLocation * getSavePosition() const;
	const bool shouldWeFlee(CUnits targets,int threshold) const;
	void escapePathPlaning();

public:
	Hitsquad(CCBot & bot, CUnit_ptr medivac);

	const bool	addMedivac(CUnit_ptr medivac);
	const bool	addMarine(CUnit_ptr marine);
	const int	getNumMarines() const;
	CUnits	getMarines() const;
	CUnit_ptr getMedivac() const;
	const int getStatus() const;
	void harass(const BaseLocation *pos = nullptr);
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


	const bool	addWidowMine(CUnit_ptr widowMine);
	CUnit_ptr getwidowMine() const;
	void harass(const sc2::Point2D pos);
};

class HarassManager
{
	CCBot &   m_bot;

	std::map<bool,Hitsquad>	m_hitSquads;
	CUnit_ptr m_liberator;
	WMHarass m_WMHarass;
	int	m_numHitSquads = 1;

	const BaseLocation * getPotentialTarget() const;
	//Medivac hit squad
	void handleHitSquads();
	//Liberator harass
	void handleLiberator();
	//Widow mine harass
	void handleWMHarass();

	

public:

	HarassManager (CCBot & bot);

	void onStart();
	void onFrame();

	const bool needMedivac() const;
	const bool needMarine() const;
	const bool needLiberator() const;
	const bool needWidowMine() const;

	const bool setMedivac(CUnit_ptr medivac);
	const bool setMarine(CUnit_ptr marine);
	const bool setLiberator(CUnit_ptr liberator);
	const bool setWidowMine(CUnit_ptr widowMine);

	CUnits getMedivacs();
	CUnits getMarines();
	CUnit_ptr getLiberator();
	CUnit_ptr getWidowMine();

};