#pragma once

#include "Common.h"
#include "BaseLocation.h"
#include <queue>
class CCBot;


namespace HarassStatus
{
	enum { Idle, Loading, OnMyWay, Harass, Fleeing, Refueling, GoingHome, Doomed };
}

class Hitsquad
{
	CCBot &   m_bot;

	int			m_status;
	const sc2::Unit * m_medivac;
	sc2::Units	m_marines;
	sc2::Units	m_doomedMarines;
	std::queue<sc2::Point2D> m_wayPoints;
	int m_pathPlanCounter;

	void checkForCasualties();
	const sc2::Unit * getTargetMarines(sc2::Units targets) const;
	const bool manhattenMove(const BaseLocation * target);
	sc2::Units getNearbyEnemyUnits() const;
	const BaseLocation * getSavePosition() const;
	const bool shouldWeFlee(sc2::Units targets) const;
	void escapePathPlaning();

public:
	Hitsquad(CCBot & bot, const sc2::Unit * medivac);

	const bool	addMedivac(const sc2::Unit * medivac);
	const bool	addMarine(const sc2::Unit * marine);
	const int	getNumMarines() const;
	sc2::Units	getMarines() const;
	const sc2::Unit * getMedivac() const;
	const int getStatus() const;
	void harass(const BaseLocation *pos);
};

class ExeBomber
{
	CCBot &   m_bot;
	const sc2::Unit * m_widowmine;

	std::queue<sc2::Point2D> m_wayPoints;
	void getWayPoints(const sc2::Point2D targetPos);
	void replanWayPoints(const sc2::Point2D targetPos);
public:
	ExeBomber(CCBot & bot);


	const bool	addWidowMine(const sc2::Unit * widowMine);
	const sc2::Unit * getwidowMine() const;
	const int getStatus() const;
	void harass(const sc2::Point2D pos);
};

class HarassManager
{
	CCBot &   m_bot;

	std::vector<Hitsquad>	m_hitSquads;
	const sc2::Unit* m_liberator;
	ExeBomber m_exeBomber;
	const int	m_numHitSquads = 1;

	std::vector<const BaseLocation *> getPotentialTargets(size_t n) const;
	//Medivac hit squad
	void handleHitSquads();
	//Liberator harass
	void handleLiberator();
	//Widow mine harass
	void handleExeBomber();

	

public:

	HarassManager (CCBot & bot);

	void onStart();
	void onFrame();

	const bool needMedivac() const;
	const bool needMarine() const;
	const bool needLiberator() const;
	const bool needWidowMine() const;

	const bool setMedivac(const sc2::Unit * medivac);
	const bool setMarine(const sc2::Unit * marine);
	const bool setLiberator(const sc2::Unit * liberator);
	const bool setWidowMine(const sc2::Unit * widowMine);

	const sc2::Unit * getMedivac();
	const sc2::Units getMarines();
	const sc2::Unit * getLiberator();
	const sc2::Unit * getWidowMine();

};