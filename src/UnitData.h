#pragma once

#include "Common.h"

struct UnitInfo
{
	// we need to store all of this data because if the unit is not visible, we
	// can't reference it from the unit pointer

	UnitTag			m_tag;
	float			m_lastHealth;
	float			m_lastShields;
	int				m_player;
	const sc2::Unit * m_unit;
	sc2::Point3D	m_lastPosition;
	sc2::UnitTypeID m_type;
	float			m_progress;

	UnitInfo()
		: m_tag(0)
		, m_lastHealth(0)
		, m_player(-1)
		, m_lastPosition(sc2::Point3D(0, 0, 0))
		, m_type(0)
		, m_progress(1.0)
	{

	}

	bool operator == (const sc2::Unit * unit) const
	{
		return m_tag == unit->tag;
	}

	bool operator == (const UnitInfo & rhs) const
	{
		return (m_tag == rhs.m_tag);
	}

	bool operator < (const UnitInfo & rhs) const
	{
		return (m_tag < rhs.m_tag);
	}
};

typedef std::vector<UnitInfo> UnitInfoVector;

class UnitData
{
	std::map<const sc2::Unit *, UnitInfo> m_unitMap;
	std::vector<int>		m_numDeadUnits;
	std::vector<int>		m_numUnits;
	int					 m_mineralsLost;
	int						m_gasLost;
	std::unordered_map<sc2::UNIT_TYPEID,sc2::Units> m_buildings;
	bool badUnitInfo(const UnitInfo & ui) const;

public:

	UnitData();

	void	updateUnit(const sc2::Unit * unit);
	void	killUnit(const sc2::Unit * unit);
	void	lostPosition(const sc2::Unit * unit);
	void	removeBadUnits();

	int		getGasLost()								const;
	int		getMineralsLost()						   const;
	int		getNumUnits(sc2::UnitTypeID t)			  const;
	int		getNumDeadUnits(sc2::UnitTypeID t)		  const;
	const	std::map<const sc2::Unit *, UnitInfo> & getUnitInfoMap()  const;
};
