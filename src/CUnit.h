#pragma once

#include "sc2api/sc2_api.h"

class CCBot;

class CUnit
{
	CCBot * m_bot;
	const sc2::Unit * m_unit;
	
	const sc2::UnitTypeID m_unitTypeId;
	float	m_healthPointsMax;
	const float m_shieldMax;
	const float m_maxEnergy;
	const bool m_isBuilding;
	const bool m_isCombatUnit;
	const float m_sight;

	sc2::Point3D m_pos;
	float	m_healthPoints;
	float 	m_shield;
	float 	m_energy;
	int		m_mineralContents;
	int		m_vespeneContents;
	bool	m_isFlying;
	bool	m_isBurrowed;
	float 	m_weaponCooldown;

	sc2::Weapon m_groundWeapons;
	sc2::Weapon m_AAWeapons;
public:

	CUnit(const sc2::Unit * unit,CCBot * bot);

	//These are always set or stay frozen
	sc2::Unit::DisplayType getDisplayType() const;
	sc2::Unit::Alliance getAlliance() const;
	sc2::Tag getTag() const;
	sc2::UnitTypeID getUnitType() const;
	int getOwner() const;
	sc2::Point3D getPos() const;
	float getFacing() const;
	float getRadius() const;
	float getBuildProgress() const;
	sc2::Unit::CloakState getCloakState() const;
	float getDetectRange() const;
	float getRadarRange() const;
	bool isSelected() const;
	bool isOnScreen() const;
	bool isBlip() const;

	//These are set to zero(?). So we need to remember
	float getHealthPoints() const;
	float getHealthPointsMax() const;
	float getShield() const;
	float getShieldMax() const;
	float getEnergy() const;
	float getEnergyMax() const;
	int getMineralContents() const;
	int getVespeneContents() const;
	bool isFlying() const;
	bool isBurrowed() const;
	float getWeaponCooldown() const;

	//Only for our units
	std::vector<sc2::UnitOrder> getOrders() const;
	sc2::Tag getAddOnTag() const;
	std::vector<sc2::PassengerUnit> getPassengers() const;
	int getCargoSpaceTaken() const;
	int getCargoSpaceMax() const;
	int getAssignedHarvesters() const;
	int getIdealHarvesters() const;
	sc2::Tag getEngagedTargetTag() const;
	std::vector<sc2::BuffID> getBuffs() const;
	bool isPowered() const;

	bool isAlive() const;
	uint32_t getLastSeenGameLoop() const;

	//Convinence
	void update();
	bool changedUnitType();
	float getHealth() const;
	float getHealthMax() const;
	bool isBuilding() const;
	bool isMineral() const;
	bool isGeyser() const;
	bool isTownHall() const;
	bool isCombatUnit() const;
	bool isWorker() const;
	bool canHitMe(const std::shared_ptr<CUnit> & enemy) const;
	std::vector<std::shared_ptr<CUnit>> getEnemyUnitsInSight() const;
	const float getSightRange() const;
	const sc2::Weapon & getWeapon(sc2::Weapon::TargetType type = sc2::Weapon::TargetType::Any) const;
	const float getAttackRange(std::shared_ptr<CUnit> target) const;
	const float getAttackRange() const;
	const sc2::Unit * getUnit_ptr() const;
	const sc2::AvailableAbilities getAbilities(const bool ignoreRequirements = false);
	const std::vector<sc2::Attribute> getAttributes() const;
	const bool hasAttribute(sc2::Attribute attribute) const;
	bool isDetector() const;
	bool IsRefineryType() const;
	bool isCompleted() const;
	bool isIdle() const;
	bool isVisible() const;
	void drawSphere() const;
};

class CUnitsData
{
	std::vector<std::shared_ptr<CUnit>> m_units;

public:
	const std::shared_ptr<CUnit> insert(const sc2::Unit * unit, CCBot & bot);
	void removeDead();
	const std::vector<std::shared_ptr<CUnit>>& getUnits() const;
};

typedef std::vector<std::shared_ptr<CUnit>> CUnits;
typedef std::shared_ptr<CUnit> CUnit_ptr;