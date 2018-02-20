#include "sc2api/sc2_api.h"

#include "CUnit.h"
#include "CCBot.h"
#include "Util.h"

CUnit::CUnit(const sc2::Unit * unit,CCBot * bot):
	m_bot(bot),
	m_unit(unit),
	m_unitTypeId(unit->unit_type),
	m_pos(unit->pos),
	m_healthPoints(unit->health),
	m_healthPointsMax(unit->health_max),
	m_shield(unit->shield),
	m_shieldMax(unit->shield_max),
	m_energy(unit->energy),
	m_maxEnergy(unit->energy_max),
	m_mineralContents(unit->mineral_contents),
	m_vespeneContents(unit->vespene_contents),
	m_isFlying(unit->is_flying),
	m_isBurrowed(unit->is_burrowed),
	m_weaponCooldown(unit->weapon_cooldown)
{

}


sc2::Unit::DisplayType CUnit::getDisplayType() const
{
	return m_unit->display_type;
}
sc2::Unit::Alliance CUnit::getAlliance() const
{
	return m_unit->alliance;
}
sc2::Tag CUnit::getTag() const
{
	return m_unit->tag;
}
sc2::UnitTypeID CUnit::getUnitType() const
{
	return m_unit->unit_type;
}
int CUnit::getOwner() const
{
	return m_unit->owner;
}
sc2::Point3D CUnit::getPos() const
{
	return m_pos;
}
float CUnit::getFacing() const
{
	return m_unit->facing;
}
float CUnit::getRadius() const
{
	return m_unit->radius;
}
float CUnit::getBuildProgress() const
{
	return m_unit->build_progress;
}
sc2::Unit::CloakState CUnit::getCloakState() const
{
	return m_unit->cloak;
}
float CUnit::getDetectRange() const
{
	return m_unit->detect_range;
}
float CUnit::getRadarRange() const
{
	return m_unit->radar_range;
}
bool CUnit::isSelected() const
{
	return m_unit->is_selected;
}
bool CUnit::isOnScreen() const
{
	return m_unit->is_on_screen;
}
bool CUnit::isBlip() const
{
	return m_unit->is_blip;
}

float CUnit::getHealthPoints() const
{
	return m_healthPoints;
}
float CUnit::getHealthPointsMax() const
{
	return m_healthPointsMax;
}
float CUnit::getShield() const
{
	return m_shield;
}
float CUnit::getShieldMax() const
{
	return m_shieldMax;
}
float CUnit::getEnergy() const
{
	return m_energy;
}
float CUnit::getEnergyMax() const
{
	return m_maxEnergy;
}
int CUnit::getMineralContents() const
{
	return m_mineralContents;
}
int CUnit::getVespeneContents() const
{
	return m_vespeneContents;
}
bool CUnit::isFlying() const
{
	return m_isFlying;
}
bool CUnit::isBurrowed() const
{
	return m_isBurrowed;
}
float CUnit::getWeaponCooldown() const
{
	return m_weaponCooldown;
}

std::vector<sc2::UnitOrder> CUnit::getOrders() const
{
	return m_unit->orders;
}
sc2::Tag CUnit::getAddOnTag() const
{
	return m_unit->add_on_tag;
}
std::vector<sc2::PassengerUnit> CUnit::getPassengers() const
{
	return m_unit->passengers;
}
int CUnit::getCargoSpaceTaken() const
{
	return m_unit->cargo_space_taken;
}
int CUnit::getCargoSpaceMax() const
{
	return m_unit->cargo_space_max;
}
int CUnit::getAssignedHarvesters() const
{
	return m_unit->assigned_harvesters;
}
int CUnit::getIdealHarvesters() const
{
	return m_unit->ideal_harvesters;
}
sc2::Tag CUnit::getEngagedTargetTag() const
{
	return m_unit->engaged_target_tag;
}
std::vector<sc2::BuffID> CUnit::getBuffs() const
{
	return m_unit->buffs;
}
bool CUnit::isPowered() const
{
	return m_unit->is_powered;
}

bool CUnit::isAlive() const
{
	return m_unit->is_alive && m_healthPoints > 0.0f;
}


uint32_t CUnit::getLastSeenGameLoop() const
{
	return m_unit->last_seen_game_loop;
}

void CUnit::update()
{	
	if (isVisible())
	{
		//DT detection
		if (m_bot->GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
		{
			const float lostHealth = m_healthPoints - m_unit->health;
			const int armor = m_bot->getArmor();
			//Immortal against Marauder gives false alarm...
			if (lostHealth == 45.0f - armor || lostHealth == 50.0f - armor || lostHealth == 55.0f - armor || lostHealth == 60.0f - armor)
			{
				m_bot->OnDTdetected(m_unit->pos);
			}
		}
		m_pos = m_unit->pos;
		m_healthPoints= m_unit->health;
		m_healthPointsMax= m_unit->health_max;
		m_shield= m_unit->shield;
		m_shieldMax= m_unit->shield_max;
		m_energy= m_unit->energy;
		m_maxEnergy= m_unit->energy_max;
		m_mineralContents= m_unit->mineral_contents;
		m_vespeneContents= m_unit->vespene_contents;
		m_isFlying= m_unit->is_flying;
		m_isBurrowed= m_unit->is_burrowed;
		m_weaponCooldown= m_unit->weapon_cooldown;
	}
	else
	{
		if (m_bot->Observation()->GetVisibility(m_pos) == sc2::Visibility::Visible)
		{
			if (isBuilding())
			{
				m_healthPoints = 0;
			}
			else if (!isBurrowed())
			{
				m_pos = sc2::Point3D();
			}
		}
	}
}

bool CUnit::changedUnitType()
{
	return m_unit->unit_type != m_unitTypeId;
}

float CUnit::getHealth() const
{
	return getHealthPoints() + getShield();
}

float CUnit::getHealthMax() const
{
	return getHealthPointsMax() + getShieldMax();
}

bool CUnit::isBuilding() const
{
	return Util::IsBuildingType(m_unit->unit_type, *m_bot);
}

bool CUnit::isMineral() const
{
	switch (getUnitType().ToType())
	{
	case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750: return true;
	default: return false;
	}
}

bool CUnit::isGeyser() const
{
	switch (getUnitType().ToType())
	{
	case sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_PURIFIERVESPENEGEYSER: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_SHAKURASVESPENEGEYSER: return true;
	case sc2::UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER: return true;
	default: return false;
	}
}

bool CUnit::isTownHall() const
{
	return Util::IsTownHallType(getUnitType());
}

bool CUnit::isCombatUnit() const
{
	return Util::IsCombatUnitType(getUnitType(), *m_bot);
}

bool CUnit::isWorker() const
{
	return Util::IsWorkerType(getUnitType());
}

bool CUnit::canHitMe(const std::shared_ptr<CUnit> enemy) const
{
	if (!enemy->isCompleted())
	{
		return false;
	}
	if (getUnitType().ToType() == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
	{
		return true;
	}
	if (isFlying())
	{
		for (const auto & weapon : m_bot->Observation()->GetUnitTypeData()[enemy->getUnitType()].weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Air || weapon.type == sc2::Weapon::TargetType::Any)
			{
				return true;
			}
		}
	}
	else
	{
		//Banelings have no weapon?
		if (enemy->getUnitType().ToType() == sc2::UNIT_TYPEID::ZERG_BANELING)
		{
			return true;
		}
		for (const auto & weapon : m_bot->Observation()->GetUnitTypeData()[enemy->getUnitType()].weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Ground || weapon.type == sc2::Weapon::TargetType::Any)
			{
				return true;
			}
		}
	}
	return false;
}

std::vector<std::shared_ptr<CUnit>> CUnit::getEnemyUnitsInSight() const
{
	std::vector<std::shared_ptr<CUnit>> enemyUnitsInSight;

	const float sightDistance = getSightRange();
	// for each enemy unit (and building?)
	for (const auto &enemy : m_bot->UnitInfo().getUnits(Players::Enemy))
	{
		const float dist = Util::Dist(getPos(), enemy->getPos());

		if (dist < sightDistance && (enemy->isCombatUnit() || enemy->isWorker()))
		{
			enemyUnitsInSight.push_back(enemy);
		}
	}
	return enemyUnitsInSight;
}

const float CUnit::getSightRange() const
{
	return m_bot->Observation()->GetUnitTypeData()[getUnitType()].sight_range;
}

const std::vector<sc2::Weapon> CUnit::getWeapons(sc2::Weapon::TargetType type) const
{
	const std::vector<sc2::Weapon> & weapons = m_bot->Observation()->GetUnitTypeData()[getUnitType()].weapons;
	if (type == sc2::Weapon::TargetType::Any)
	{
		return weapons;
	}
	for (const auto & weapon : weapons)
	{
		if (weapon.type == type || weapon.type == sc2::Weapon::TargetType::Any)
		{
			return { weapon };
		}
	}
	return std::vector<sc2::Weapon>();
}

const float CUnit::getAttackRange(std::shared_ptr<CUnit> target) const
{
	const auto & weapons = m_bot->Observation()->GetUnitTypeData()[getUnitType()].weapons;

	if (weapons.empty())
	{
		if (getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			return 4.0f;
		}
		return 0.0f;
	}

	if (target->isFlying())
	{
		for (const auto & weapon : weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Air || weapon.type == sc2::Weapon::TargetType::Any)
			{
				return weapon.range;
			}
		}
	}
	else
	{
		for (const auto & weapon : weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Ground || weapon.type == sc2::Weapon::TargetType::Any)
			{
				return weapon.range;
			}
		}
	}
	return 0.0f;
}

const float CUnit::getAttackRange() const
{
	const auto & weapons = m_bot->Observation()->GetUnitTypeData()[getUnitType()].weapons;

	if (weapons.empty())
	{
		if (getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			return 4.0f;
		}
		return 0.0f;
	}

	float maxRange = 0.0f;
	for (const auto & weapon : weapons)
	{
		if (maxRange < weapon.range)
		{
			maxRange = weapon.range;
		}
	}
	return maxRange;
}

const sc2::Unit * CUnit::getUnit_ptr() const
{
	const sc2::Unit * unit_ptr = m_bot->Observation()->GetUnit(getTag());
	return unit_ptr ? unit_ptr:m_unit;
}

const sc2::AvailableAbilities CUnit::getAbilities(const bool ignoreRequirements)
{
	return m_bot->Query()->GetAbilitiesForUnit(getUnit_ptr(), ignoreRequirements);
}

const std::vector<sc2::Attribute> CUnit::getAttributes() const
{
	return m_bot->Observation()->GetUnitTypeData()[getUnitType()].attributes;
}

const bool CUnit::hasAttribute(sc2::Attribute attribute) const
{
	const std::vector<sc2::Attribute> attributes = getAttributes();
	return std::find(attributes.begin(), attributes.end(), sc2::Attribute::Biological) != attributes.end();
}

bool CUnit::isDetector() const
{
	switch (getUnitType().ToType())
	{
	case sc2::UNIT_TYPEID::PROTOSS_OBSERVER: return true;
	case sc2::UNIT_TYPEID::ZERG_OVERSEER: return true;
	case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET: return true;
	case sc2::UNIT_TYPEID::ZERG_SPORECRAWLER: return true;
	case sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON: return true;
	case sc2::UNIT_TYPEID::TERRAN_RAVEN: return true;
	default: return false;
	}
}

bool CUnit::IsRefineryType() const
{
	switch (getUnitType().ToType())
	{
	case sc2::UNIT_TYPEID::TERRAN_REFINERY: return true;
	case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR: return true;
	case sc2::UNIT_TYPEID::ZERG_EXTRACTOR: return true;
	default: return false;
	}
}

bool CUnit::isCompleted() const
{
	return getBuildProgress() == 1.0f;
}

bool CUnit::isIdle() const
{
	return getOrders().empty();
}

bool CUnit::isVisible() const
{
	return getDisplayType() != sc2::Unit::DisplayType::Snapshot && getLastSeenGameLoop() == m_bot->Observation()->GetGameLoop();
}

void CUnit::drawSphere() const
{
	switch (getAlliance())
	{
	case (sc2::Unit::Alliance::Self):
	{
		Drawing::drawSphereAroundUnit(*m_bot, m_unit->tag, sc2::Colors::Green);
		break;
	}
	case (sc2::Unit::Alliance::Enemy):
	{
		Drawing::drawSphere(*m_bot, m_pos, getRadius(), sc2::Colors::Red);
		break;
	}
	case (sc2::Unit::Alliance::Neutral):
	{
		Drawing::drawSphereAroundUnit(*m_bot, m_unit->tag, sc2::Colors::Gray);
		break;
	}
	}
	Drawing::drawText(*m_bot, m_pos, sc2::UnitTypeToName(getUnitType()));
}


///////////////////////////////////////////////////////////////////

const std::vector<std::shared_ptr<CUnit>> &CUnitsData::getUnits() const
{
	return m_units;
}

const std::shared_ptr<CUnit> CUnitsData::insert(const sc2::Unit * unit, CCBot & bot)
{
	for (const auto & u : m_units)
	{
		if (u->getTag() == unit->tag)
		{
			u->update();
			return nullptr;
		}
	}
	m_units.push_back(std::make_shared<CUnit>(unit, &bot));
	return m_units.back();
}

void CUnitsData::removeDead()
{
	m_units.erase(std::remove_if(m_units.begin(), m_units.end(), [](std::shared_ptr<CUnit> & unit) {
		if (!unit)
		{
			return true;
		}
		if (useDebug)
		{
			unit->drawSphere();
		}
		if (unit->isVisible())
		{
			//Change in unit type means it is now in another vector
			return !unit->isAlive() || unit->changedUnitType();
		}
		else
		{
			unit->update();
			return false;
		}
	}), m_units.end());
}
