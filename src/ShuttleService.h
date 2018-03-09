#pragma once
#include "sc2api/sc2_api.h"
#include "CUnit.h"
#include "Common.h"
#include <queue>

class CCBot;

namespace ShuttleStatus
{
	enum {lookingForShuttle,Loading, OnMyWay, Unloading, OnMyWayBack, Done};
}

class shuttle
{
	CCBot * m_bot;
	CUnit_ptr m_shuttle;
	CUnits m_passengers;
	sc2::Point2D m_targetPos;
	int m_status;
	std::queue<sc2::Point2D> m_wayPoints;

	void loadPassangers();
	void travelToDestination();
	void travelBack();
	void unloadPassangers();

public:
	shuttle(CCBot * const bot, CUnits passengers, sc2::Point2D targetPos);

	//Really bad at naming things rn
	void hereItGoes();
	const bool isShuttle(CUnit_ptr unit) const;
	void updateTargetPos(const sc2::Point2D newTargetPos);
	const bool needShuttleUnit() const;
	void assignShuttleUnit(CUnit_ptr unit);
	int getShuttleStatus() const;
};