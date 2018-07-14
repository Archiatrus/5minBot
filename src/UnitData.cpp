#include "UnitData.h"
#include "Util.h"

UnitData::UnitData()
	: m_mineralsLost(0)
	, m_gasLost(0)
{
	const int maxTypeID = 1024;
	m_numDeadUnits		= std::vector<int>(maxTypeID + 1, 0);
	m_numUnits			= std::vector<int>(maxTypeID + 1, 0);
}

/*
void UnitData::updateUnit(const sc2::Unit * unit)
{

	bool firstSeen = false;
	const auto & it = m_unitMap.find(unit);
	if (it == m_unitMap.end())
	{
		firstSeen = true;
		m_unitMap[unit] = UnitInfo();
	}

	UnitInfo & ui = m_unitMap[unit];
	ui.m_unit = unit;
	ui.m_player = Util::GetPlayer(unit);
	ui.m_lastPosition = unit->pos;
	ui.m_lastHealth = unit->health;
	ui.m_lastShields = unit->shield;
	ui.m_tag = unit->tag;
	ui.m_type = unit->unit_type;
	ui.m_progress = unit->build_progress;
	if (firstSeen)
	{
		m_numUnits[ui.m_type]++;
	}
}
*/

void UnitData::killUnit(const sc2::Unit * unit)
{
	//_mineralsLost += unit->getType().mineralPrice();
	//_gasLost += unit->getType().gasPrice();
	m_numUnits[unit->unit_type]--;
	m_numDeadUnits[unit->unit_type]++;

	m_unitMap.erase(unit);
	//m_buildings[unit->unit_type.ToType()].erase(unit);
}

void UnitData::lostPosition(const sc2::Unit * unit)
{
	const auto & it = m_unitMap.find(unit);
	BOT_ASSERT(it != m_unitMap.end(), "We should not have a snapshot of unit we have never seen!");

	UnitInfo & ui = m_unitMap[unit];
	ui.m_lastPosition = sc2::Point3D(0.0f,0.0f,0.0f);
}

void UnitData::removeBadUnits()
{
	for (auto iter = m_unitMap.begin(); iter != m_unitMap.end();)
	{
		if (badUnitInfo(iter->second))
		{
			m_numUnits[iter->second.m_type]--;
			iter = m_unitMap.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

bool UnitData::badUnitInfo(const UnitInfo & ui) const
{
	//!ui.unit->is_alive does not seem to work for spines?!
	if (!ui.m_unit->is_alive || ui.m_lastHealth==0.0f)
	{
		return true;
	}
	return false;
}

int UnitData::getGasLost() const
{
	return m_gasLost;
}

int UnitData::getMineralsLost() const
{
	return m_mineralsLost;
}

int UnitData::getNumUnits(sc2::UnitTypeID t) const
{
	return m_numUnits[t];
}

int UnitData::getNumDeadUnits(sc2::UnitTypeID t) const
{
	return m_numDeadUnits[t];
}

const std::map<const sc2::Unit *, UnitInfo> & UnitData::getUnitInfoMap() const
{
	return m_unitMap;
}