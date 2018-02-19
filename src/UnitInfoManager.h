#pragma once

#include "sc2api/sc2_api.h"

#include "BaseLocation.h"
#include "CUnit.h"

class CCBot;
class UnitInfoManager 
{
    CCBot &           m_bot;

	std::map<int, std::map<sc2::UnitTypeID, CUnitsData>> m_unitDataBase;

    std::map<int, std::vector<const sc2::Unit *>> m_units;

    void                    updateUnitInfo();
    bool                    isValidUnit(const sc2::Unit * unit);




    void drawSelectedUnitDebugInfo();

public:

    UnitInfoManager(CCBot & bot);

    void                    onFrame();
    void                    onStart();


    const size_t                  getUnitTypeCount(int player, sc2::UnitTypeID type, bool completed = true) const;

	const size_t getUnitTypeCount(int player, std::vector<sc2::UnitTypeID> types, bool completed = true) const;

    //bool                  enemyHasCloakedUnits() const;
	const size_t getNumCombatUnits(int player) const;

	const CUnit_ptr OnUnitCreate(const sc2::Unit * unit);
	const std::shared_ptr<CUnit> getUnit(sc2::Tag unitTag);
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, sc2::UnitTypeID type) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, std::vector<sc2::UnitTypeID> types) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, std::vector<sc2::UNIT_TYPEID> types) const;
};

extern bool useDebug;