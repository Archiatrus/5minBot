#pragma once
#include "sc2api/sc2_api.h"
#include "Common.h"
#include <queue>

class CCBot;

namespace ShuttleStatus
{
	enum {lookingForShuttle,Loading, OnMyWay, Unloading, Done};
}

class shuttle
{
	CCBot * m_bot;
	const sc2::Unit* m_shuttle;
	sc2::Units m_passengers;
	sc2::Point2D m_targetPos;
	int m_status;
	std::queue<sc2::Point2D> m_wayPoints;

	void loadPassangers();
	void travelToDestination();
	void unloadPassangers();

public:
	shuttle(CCBot * const bot, sc2::Units passengers, sc2::Point2D targetPos);

	//Really bad at naming things rn
	void hereItGoes();
	const bool isShuttle(const sc2::Unit* unit) const;
	void updateTargetPos(const sc2::Point2D newTargetPos);
	const bool needShuttleUnit() const;
	void assignShuttleUnit(const sc2::Unit* unit);
	int getShuttleStatus() const;
};