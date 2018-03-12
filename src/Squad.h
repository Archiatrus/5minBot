#pragma once

#include "Common.h"
#include "MeleeManager.h"
#include "RangedManager.h"
#include "SiegeManager.h"
#include "SquadOrder.h"

class CCBot;

class Squad
{
	CCBot &             m_bot;

	std::string         m_name;
	CUnits m_units;
	std::string         m_regroupStatus;
	int                 m_lastRetreatSwitch;
	bool                m_lastRetreatSwitchVal;
	size_t              m_priority;

	SquadOrder          m_order;
	MeleeManager        m_meleeManager;
	RangedManager       m_rangedManager;
	SiegeManager		m_siegeManager;

	std::map<const CUnit_ptr, bool> m_nearEnemy;

	const CUnit_ptr unitClosestToEnemy() const;

	void updateUnits();
	void addUnitsToMicroManagers();
	void setNearEnemyUnits();
	void setAllUnits();

	bool isUnitNearEnemy(const CUnit_ptr unit) const;
	const bool needsToRegroup();
	int  squadUnitsNear(const sc2::Point2D & pos) const;

public:

	Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot);
	Squad(CCBot & bot);

	void onFrame();
	void setSquadOrder(const SquadOrder & so);
	void addUnit(const CUnit_ptr unit);
	void removeUnit(const CUnit_ptr unit);
	void clear();

	bool containsUnit(const CUnit_ptr unit) const;
	bool isEmpty() const;
	size_t getPriority() const;
	void setPriority(const size_t & priority);
	const std::string & getName() const;

	sc2::Point2D calcCenter() const;
	sc2::Point2D calcRegroupPosition() const;

	const CUnits & getUnits() const;
	const SquadOrder & getSquadOrder() const;
};

extern bool useDebug;