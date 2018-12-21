#pragma once

#include "sc2api/sc2_api.h"

#include "BaseLocation.h"
#include "CUnit.h"

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/box.hpp>

namespace bgi = boost::geometry::index;

struct RTreeNode
{
	CUnit_ptr unit = nullptr;
	sc2::Point2D position{};
	RTreeNode(const sc2::Point2D& position) : position(position) {}
	RTreeNode(const CUnit_ptr& unit) : unit(unit),position(unit->getPos()) {}
};

BOOST_GEOMETRY_REGISTER_POINT_2D(RTreeNode, double_t, boost::geometry::cs::cartesian, position.x, position.y)

using RTree = bgi::rtree<RTreeNode, bgi::quadratic<32>>;


class CCBot;
class UnitInfoManager 
{
	CCBot &		   m_bot;

	std::map<int, std::map<sc2::UnitTypeID, CUnitsData>> m_unitDataBase;

	std::map<int, std::vector<const sc2::Unit *>> m_units;
	std::map<sc2::Unit::Alliance,RTree> m_rtree;

    void                    updateUnitInfo();
    bool                    isValidUnit(const sc2::Unit * unit);



	void drawSelectedUnitDebugInfo();

public:

	UnitInfoManager(CCBot & bot);

	void					onFrame();
	void					onStart();


    size_t                  getUnitTypeCount(int player, sc2::UnitTypeID type, bool completed = true) const;

    size_t getUnitTypeCount(int player, std::vector<sc2::UnitTypeID> types, bool completed = true) const;
    size_t getUnitTypeCount(int player, std::vector<sc2::UNIT_TYPEID> types, bool completed = true) const;

    //bool                  enemyHasCloakedUnits() const;
    size_t getNumCombatUnits(int player) const;

    size_t getFoodCombatUnits(int player) const;

	const CUnit_ptr OnUnitCreate(const sc2::Unit * unit);
	const std::shared_ptr<CUnit> getUnit(sc2::Tag unitTag);
	const std::shared_ptr<CUnit> getUnit(const sc2::Unit * unit);
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, sc2::UnitTypeID type) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, std::vector<sc2::UnitTypeID> types) const;
	const std::vector<std::shared_ptr<CUnit>> getUnits(int player, std::vector<sc2::UNIT_TYPEID> types) const;
	std::vector<std::shared_ptr<CUnit>> getUnitsNear(int player, const CUnit_ptr& unit, size_t k) const;
	std::vector<std::shared_ptr<CUnit>> getUnitsNear(int player, const sc2::Point2D& pos, size_t k) const;
};

extern bool useDebug;
