#pragma once
#include "Common.h"
#include "BaseLocation.h"
#include "CUnit.h"
#include <queue>

class CCBot;

class HarassLiberator
{
public:
	enum class Status { init, idle, onMyWay, Harass, Dead };

 private:
	CCBot & m_bot;

	Status m_status;
	CUnit_ptr m_liberator;
	std::queue<sc2::Point2D> m_wayPoints;
	int m_pathPlanCounter;
	const BaseLocation *m_target;
	sc2::Point2D m_targetSpot;
	sc2::Point2D m_stalemateCheck;

	void planPath();

	void travelToDestination();

	void ActivateSiege();

 public:
	HarassLiberator(CCBot & bot);
	~HarassLiberator();

	const bool needLiberator();

	const Status getStatus() const;
	const bool setLiberator(const CUnit_ptr liberator);
	bool harass(const BaseLocation *pos = nullptr);
};
