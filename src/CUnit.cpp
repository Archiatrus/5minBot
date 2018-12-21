#pragma GCC diagnostic ignored "-Wenum-compare"

#include "CUnit.h"
#include "sc2api/sc2_api.h"
#include "CCBot.h"
#include "Util.h"

CUnit::CUnit(const sc2::Unit * unit, CCBot * bot):
	m_bot(bot),
	m_unit(unit),
	m_unitTypeId(unit->unit_type),
	m_healthPointsMax(unit->health_max),
	m_shieldMax(unit->shield_max),
	m_maxEnergy(unit->energy_max),
	m_isBuilding([&]
		{
			const auto attributes = this->getAttributes();
			return std::find(attributes.begin(), attributes.end(),
				sc2::Attribute::Structure) != attributes.end()
				&& !isType(sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
				&& !isType(sc2::UNIT_TYPEID::TERRAN_AUTOTURRET)
				&& !isType(sc2::UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE)
				&& !isType(sc2::UNIT_TYPEID::ZERG_CREEPTUMOR)
				&& !isType(sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED)
				&& !isType(sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN);
		}()),
	m_isCombatUnit(unit->alliance != sc2::Unit::Alliance::Neutral && Util::IsCombatUnitType(unit->unit_type, *bot)),
	m_sight(m_bot->Observation()->GetUnitTypeData()[getUnitType()].sight_range),
	m_pos(unit->pos),
	m_healthPoints(unit->health),
	m_shield(unit->shield),
	m_energy(unit->energy),
	m_mineralContents(unit->mineral_contents),
	m_vespeneContents(unit->vespene_contents),
	m_isFlying(unit->is_flying),
	m_isBurrowed(Util::IsBurrowedType(unit->unit_type)),
	m_weaponCooldown(unit->weapon_cooldown),
	m_abilityCooldown(bot->Observation()->GetGameLoop()),
	m_AAWeapons(sc2::Weapon{}),
	m_groundWeapons(sc2::Weapon{}),
	m_occupation([&]() -> std::pair<CUnit::Occupation, int>
		{
			if (this->isWorker())
				{
					return { Occupation::Building, 0 };
				}
			if (m_isBuilding)
				{
					return { Occupation::Building, 0 };
				}
			if (m_isCombatUnit)
				{
					return { Occupation::Combat, 0 };
				}
			return { Occupation::None, 0 };
		}()),
	m_maybeBanshee(0),
	m_reachable(true)

{
	// The default constructor is not setting the values...
	m_AAWeapons.type = sc2::Weapon::TargetType::Air;
	m_AAWeapons.damage_ = 0.0f;
	m_AAWeapons.attacks = 0;
	m_AAWeapons.range = 0.0f;
	m_AAWeapons.speed = 1.0f;  // For dps I divide dmg/speed
	m_groundWeapons.type = sc2::Weapon::TargetType::Ground;
	m_groundWeapons.damage_ = 0.0f;
	m_groundWeapons.attacks = 0;
	m_groundWeapons.range = 0.0f;
	m_groundWeapons.speed = 1.0f;  // For dps I divide dmg/speed
	for (const auto & weapon : bot->Observation()->GetUnitTypeData()[unit->unit_type].weapons)
	{
		if (weapon.type == sc2::Weapon::TargetType::Air || weapon.type == sc2::Weapon::TargetType::Any)
		{
			m_AAWeapons = weapon;
			if (isType(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET) || isType(sc2::UNIT_TYPEID::ZERG_HYDRALISK))
			{
				m_AAWeapons.range += 1.0f;  // We never know when range is researched
			}
			else if (isType(sc2::UNIT_TYPEID::PROTOSS_PHOENIX))
			{
				m_AAWeapons.range += 2.0f;
			}
		}
		if ((weapon.type == sc2::Weapon::TargetType::Ground || weapon.type == sc2::Weapon::TargetType::Any)
			&& weapon.range > m_groundWeapons.range)  // Siege tanks
		{
			m_groundWeapons = weapon;
			if (isType(sc2::UNIT_TYPEID::ZERG_HYDRALISK))
			{
				m_groundWeapons.range += 1.0f;  // We never know when range is researched
			}
			else if (isType(sc2::UNIT_TYPEID::PROTOSS_COLOSSUS))
			{
				m_groundWeapons.range += 2.0f;
			}
		}
	}
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
const sc2::Point3D &CUnit::getPos() const
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
size_t CUnit::getCargoSpaceTaken() const
{
    return static_cast<size_t>(m_unit->cargo_space_taken);
}
size_t CUnit::getCargoSpaceMax() const
{
    return static_cast<size_t>(m_unit->cargo_space_max);
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
std::shared_ptr<CUnit> CUnit::getTarget()
{
	return m_bot->UnitInfo().getUnit(getEngagedTargetTag());
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
	return m_unit->is_alive && (m_healthPointsMax == 0 || m_healthPoints > 0.0f);
}


uint32_t CUnit::getLastSeenGameLoop() const
{
	return m_unit->last_seen_game_loop;
}


std::shared_ptr<CUnit> CUnit::getAddOn() const
{
    return m_bot->UnitInfo().getUnit(getAddOnTag());
}

void CUnit::update()
{
	if (isVisible())
	{
		if (getAlliance() == Players::Self)
		{
			if (m_bot->GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
			{
				// DT detection
				const float lostHealth = m_healthPoints - m_unit->health;
				const int armor = static_cast<int>(m_bot->Observation()->GetUnitTypeData()[getUnitType()].armor);
				if (lostHealth == 45.0f - armor || lostHealth == 50.0f - armor || lostHealth == 55.0f - armor || lostHealth == 60.0f - armor)
				{
					m_bot->OnCloakDetected(sc2::UNIT_TYPEID::PROTOSS_DARKTEMPLAR, m_unit->pos);
				}
			}
			else if (m_bot->GetPlayerRace(Players::Enemy) == sc2::Race::Terran)
			{
				// Banshee detection
				const float lostHealth = m_healthPoints - m_unit->health;
				const int armor = static_cast<int>(m_bot->Observation()->GetUnitTypeData()[getUnitType()].armor);
				if (lostHealth == 12.0f - armor || lostHealth == 13.0f - armor || lostHealth == 14.0f - armor || lostHealth == 15.0f - armor)
				{
					const uint32_t currentTime = m_bot->Observation()->GetGameLoop();
					if (currentTime - m_maybeBanshee <= 2)
					{
						bool hiddenBanshee = true;
						for (const auto & unit : m_bot->UnitInfo().getUnits(Players::Enemy, sc2::UNIT_TYPEID::TERRAN_BANSHEE))
						{
							if (Util::DistSq(unit->getPos(), getPos()) < std::pow(unit->getAttackRange() + 0.5f, 2))
							{
								hiddenBanshee = false;
								break;
							}
						}
						if (hiddenBanshee)
						{
							m_bot->OnCloakDetected(sc2::UNIT_TYPEID::TERRAN_BANSHEE, m_unit->pos);
						}
					}
					m_maybeBanshee = currentTime;
				}
			}
		}
		m_pos = m_unit->pos;
		m_healthPoints = m_unit->health;
		m_healthPointsMax = m_unit->health_max;  // Combat shield
		m_shield = m_unit->shield;
		m_energy = m_unit->energy;
		m_mineralContents = m_unit->mineral_contents;
		m_vespeneContents = m_unit->vespene_contents;
		m_isFlying = m_unit->is_flying;
		m_isBurrowed = m_unit->is_burrowed;
		m_weaponCooldown = m_unit->weapon_cooldown;
	}
	else
	{
		if (m_bot->Observation()->GetVisibility(m_pos) == sc2::Visibility::Visible && getAlliance() != sc2::Unit::Alliance::Neutral)
		{
			if (isBuilding())
			{
				m_healthPoints = 0;
			}
			else if (!isBurrowed())
			{
				m_pos = sc2::Point3D();
			}
			else if (isBurrowed())
			{
				m_bot->OnCloakDetected(getUnitType(), getPos());
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
	return m_isBuilding;
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
	return m_isCombatUnit;
}

bool CUnit::isWorker() const
{
	return Util::IsWorkerType(getUnitType());
}

bool CUnit::canHitMe(const std::shared_ptr<CUnit> & enemy) const
{
	if (!enemy || !enemy->isAlive() || !enemy->isCompleted() || (isBurrowed() && !isVisible()))
	{
		return false;
	}
	if (getUnitType().ToType() == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
	{
		return true;
	}
	if (isFlying())
	{
		return enemy->getWeapon(sc2::Weapon::TargetType::Air).damage_ > 0.0f;
	}
	return enemy->getWeapon(sc2::Weapon::TargetType::Ground).damage_ > 0.0f;
}

float CUnit::hasBonusDmgAgainst(const std::shared_ptr<CUnit> & enemy) const
{
	if (!enemy || !enemy->isAlive() || !enemy->isCompleted() || (isBurrowed() && !isVisible()))
	{
		return false;
	}
	for (const auto & bonus : getWeapon(sc2::Weapon::TargetType::Air).damage_bonus)
	{
		if (enemy->hasAttribute(bonus.attribute))
		{
			return bonus.bonus;
		}
	}
	return 0.0f;
}

std::vector<std::shared_ptr<CUnit>> CUnit::getClosestUnits(const sc2::Unit::Alliance alliance,const size_t k) const
{
	std::vector<std::shared_ptr<CUnit>> nearestUnits;
	nearestUnits.reserve(k);
	return nearestUnits;
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

float CUnit::getSightRange() const
{
	return m_sight;
}

const sc2::Weapon & CUnit::getWeapon(sc2::Weapon::TargetType type) const
{
	if (type == sc2::Weapon::TargetType::Ground || type == sc2::Weapon::TargetType::Any)
	{
		return m_groundWeapons;
	}
	return m_AAWeapons;
}

float CUnit::getAttackRange(const std::shared_ptr<CUnit> & target) const
{
	if (getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
	{
		return 4.0f;
	}
	if (target->isFlying())
	{
				return m_AAWeapons.range;
	}
	else
	{
		return m_groundWeapons.range;
	}
}

float CUnit::getAttackRange(const sc2::Weapon::TargetType &target) const
{
	if (target == sc2::Weapon::TargetType::Air)
	{
		return m_AAWeapons.range;
	}
	else
	{
		return m_groundWeapons.range;
	}
}

float CUnit::getAttackRange() const
{
	if (getUnitType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
	{
		return 4.0f;
	}
	return m_groundWeapons.range > m_AAWeapons.range ? m_groundWeapons.range : m_AAWeapons.range;
}

float CUnit::getMovementSpeed() const
{
	if (getAlliance() == Players::Enemy)
	{
		switch (getUnitType().ToType())
		{
		case (sc2::UNIT_TYPEID::PROTOSS_ZEALOT): return 2.75f;

		case (sc2::UNIT_TYPEID::TERRAN_MARINE): return 3.375f;
		case (sc2::UNIT_TYPEID::TERRAN_MARAUDER): return 3.375f;

		case (sc2::UNIT_TYPEID::ZERG_ZERGLING): { return isOnCreep() ? 6.10883f : 4.6991f; }
		case (sc2::UNIT_TYPEID::ZERG_ROACH): { return isOnCreep() ? 3.9f : 3.0f; }
		case (sc2::UNIT_TYPEID::ZERG_HYDRALISK): { return isOnCreep() ? 3.657142f : 2.8125f; }
		case (sc2::UNIT_TYPEID::ZERG_BANELING): { return isOnCreep() ? 3.83903f : 2.9531f; }
		case (sc2::UNIT_TYPEID::ZERG_ULTRALISK): { return isOnCreep() ? 3.83903f : 2.9531f; }
		case (sc2::UNIT_TYPEID::ZERG_INFESTOR): { return isOnCreep() ? 2.925f : 2.25f; }
		case (sc2::UNIT_TYPEID::ZERG_SWARMHOSTMP): { return isOnCreep() ? 2.925f : 2.25f; }
		case (sc2::UNIT_TYPEID::ZERG_INFESTORTERRAN): { return isOnCreep() ? 1.21875f : 0.9375f; }
		case (sc2::UNIT_TYPEID::ZERG_QUEEN): { return isOnCreep() ? 2.5f : 0.9375f; }
        default: {}
		}
	}
	return m_bot->Observation()->GetUnitTypeData()[getUnitType()].movement_speed;
}

const sc2::Unit * CUnit::getUnit_ptr() const
{
	const sc2::Unit * unit_ptr = m_bot->Observation()->GetUnit(getTag());
	return unit_ptr ? unit_ptr:m_unit;
}

void CUnit::testReachable()
{
	if (isBuilding() && getAlliance() == Players::Enemy)
	{
		auto worker = m_bot->Workers().getClosestMineralWorkerTo(getPos());
		if (worker)
		{
			const float dist = m_bot->Query()->PathingDistance(worker->getUnit_ptr(), getPos());
			m_reachable = dist > 0;
		}
	}
}

bool CUnit::isReachable() const
{
	return m_reachable;
}

const sc2::AvailableAbilities CUnit::getAbilities(const bool ignoreRequirements)
{
	return m_bot->Query()->GetAbilitiesForUnit(getUnit_ptr(), ignoreRequirements);
}

const std::vector<sc2::Attribute> CUnit::getAttributes() const
{
	return m_bot->Observation()->GetUnitTypeData()[getUnitType()].attributes;
}

bool CUnit::hasAttribute(sc2::Attribute attribute) const
{
	const std::vector<sc2::Attribute> attributes = getAttributes();
	return std::find(attributes.begin(), attributes.end(), attribute) != attributes.end();
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

bool CUnit::isType(sc2::UnitTypeID type) const
{
    if (type == sc2::UNIT_TYPEID::TERRAN_TECHLAB)
    {
        return m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB || m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB || m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB;
    }
    if (type == sc2::UNIT_TYPEID::TERRAN_REACTOR)
    {
        return m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR || m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR || m_unitTypeId == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR;
    }
	return m_unitTypeId == type;
}

bool CUnit::isOnCreep() const
{
	return m_bot->Observation()->HasCreep(getPos());
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
    default:
    {
        Drawing::drawSphereAroundUnit(*m_bot, m_unit->tag, sc2::Colors::Yellow);
        break;
    }
	}
	std::string name = sc2::UnitTypeToName(getUnitType());
	if (name == "UNKNOWN")
	{
		Drawing::drawText(*m_bot, m_pos, std::to_string(getUnitType()));
	}
	else
	{
		Drawing::drawText(*m_bot, m_pos, sc2::UnitTypeToName(getUnitType()));
	}
}

uint32_t CUnit::getAbilityCoolDown() const
{
	return m_abilityCooldown;
}

void CUnit::newAbilityCoolDown(const uint32_t CD)
{
	m_abilityCooldown = CD;
}

const std::pair<CUnit::Occupation, int> CUnit::getOccupation() const
{
	return m_occupation;
}

void CUnit::setOccupation(std::pair<CUnit::Occupation, int> occupation)
{
	m_occupation = occupation;
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
		// Snapshots have different tags -.-
		if (u->getDisplayType() == sc2::Unit::DisplayType::Snapshot && unit->display_type == sc2::Unit::DisplayType::Visible && Util::Dist(u->getPos(), unit->pos) <= 0.1f)
		{
			m_units.erase(std::find(m_units.begin(), m_units.end(), u));
			break;
		}
	}
	m_units.push_back(std::make_shared<CUnit>(unit, &bot));
	if (bot.GetPlayerRace(Players::Enemy) == sc2::Race::Random && unit->alliance == sc2::Unit::Alliance::Enemy)
	{
		bot.setPlayerRace(Players::Enemy, bot.Observation()->GetUnitTypeData()[unit->unit_type].race);
	}
	return m_units.back();
}

void CUnitsData::removeDead(CCBot & bot)
{
	m_units.erase(std::remove_if(m_units.begin(), m_units.end(), [&](std::shared_ptr<CUnit> & unit) {
		if (!unit)
		{
			return true;
		}
		unit->drawSphere();
		if (unit->getDisplayType() == sc2::Unit::DisplayType::Snapshot)
		{
			return false;
		}
		if (!unit->isAlive())
		{
			return true;
		}
		if (unit->changedUnitType())
		{
			const CUnit_ptr newUnit = bot.UnitInfo().getUnit(unit->getTag());
			newUnit->setOccupation(unit->getOccupation());
			if (Util::IsBurrowedType(newUnit->getUnitType()))
			{
				bot.OnCloakDetected(newUnit->getUnitType(), newUnit->getPos());
			}
			return true;
		}
		if (!unit->isVisible())
		{
			unit->update();
		}
		if (bot.Observation()->GetGameLoop() % 1000 == unit->getTag() % 1000)
		{
			unit->testReachable();
		}
		return false;
	}), m_units.end());
}
