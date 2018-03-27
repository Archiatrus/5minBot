#pragma once

#include "Common.h"
#include "SquadOrder.h"
#include "Micro.h"

struct AirThreat
{
	UnitTag	m_unit;
	double weight;
};

struct GroundThreat
{
	UnitTag	m_unit;
	double weight;
};

class CCBot;

class MicroManager
{
	CUnits m_units;

protected:

	CCBot & m_bot;
	SquadOrder order;

	virtual void executeMicro(const CUnits & targets) = 0;
	void trainSubUnits(CUnit_ptr unit) const;


public:

	MicroManager(CCBot & bot);

	const CUnits & getUnits() const;

	void setUnits(CUnits & u);
	void execute(const SquadOrder & order);
	void regroup(const sc2::Point2D & regroupPosition) const;

};