#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"
#include "Util.h"
#include "CCBot.h"
#include <iostream>


Util::IsUnit::IsUnit(sc2::UNIT_TYPEID type) 
    : m_type(type) 
{
}
 
bool Util::IsUnit::operator()(const sc2::Unit * unit, const sc2::ObservationInterface*) 
{ 
    return unit->unit_type == m_type; 
};

bool Util::IsTownHallType(const sc2::UnitTypeID & type)
{
    switch (type.ToType()) 
    {
        case sc2::UNIT_TYPEID::ZERG_HATCHERY                : return true;
        case sc2::UNIT_TYPEID::ZERG_LAIR                    : return true;
        case sc2::UNIT_TYPEID::ZERG_HIVE                    : return true;
        case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER         : return true;
        case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND        : return true;
        case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING  : return true;
        case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS     : return true;
        case sc2::UNIT_TYPEID::PROTOSS_NEXUS                : return true;
        default: return false;
    }
}

bool Util::IsTownHall(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsTownHallType(unit->unit_type);
}

bool Util::IsRefinery(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsRefineryType(unit->unit_type);
}

bool Util::IsRefineryType(const sc2::UnitTypeID & type)
{
    switch (type.ToType()) 
    {
        case sc2::UNIT_TYPEID::TERRAN_REFINERY      : return true;
        case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR  : return true;
        case sc2::UNIT_TYPEID::ZERG_EXTRACTOR       : return true;
        default: return false;
    }
}

bool Util::IsGeyser(const sc2::Unit * unit)
{
	BOT_ASSERT(unit, "Unit pointer was null");
	switch (unit->unit_type.ToType())
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

bool Util::IsMineral(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    switch (unit->unit_type.ToType()) 
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

bool Util::IsWorker(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsWorkerType(unit->unit_type);
}

bool Util::IsWorkerType(const sc2::UnitTypeID & unit)
{
    switch (unit.ToType()) 
    {
        case sc2::UNIT_TYPEID::TERRAN_SCV           : return true;
		case sc2::UNIT_TYPEID::TERRAN_MULE			: return true;
        case sc2::UNIT_TYPEID::PROTOSS_PROBE        : return true;
        case sc2::UNIT_TYPEID::ZERG_DRONE           : return true;
        case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED   : return true;
        default: return false;
    }
}

bool Util::IsBuildingType(const sc2::UnitTypeID & type, const CCBot &bot)
{
	return bot.Data(type).isBuilding;
}
sc2::UnitTypeID Util::GetSupplyProvider(const sc2::Race & race)
{
    switch (race) 
    {
        case sc2::Race::Terran: return sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT;
        case sc2::Race::Protoss: return sc2::UNIT_TYPEID::PROTOSS_PYLON;
        case sc2::Race::Zerg: return sc2::UNIT_TYPEID::ZERG_OVERLORD;
        default: return 0;
    }
}

sc2::UnitTypeID Util::GetTownHall(const sc2::Race & race)
{
    switch (race) 
    {
        case sc2::Race::Terran: return sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER;
        case sc2::Race::Protoss: return sc2::UNIT_TYPEID::PROTOSS_NEXUS;
        case sc2::Race::Zerg: return sc2::UNIT_TYPEID::ZERG_HATCHERY;
        default: return 0;
    }
}

bool Util::IsCompleted(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return unit->build_progress == 1.0f;
}

bool Util::IsIdle(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return unit->orders.empty();
}

const BaseLocation * Util::getClosestBase(sc2::Point2D pos,const CCBot &bot)
{
	const std::vector<const BaseLocation *> bases = bot.Bases().getBaseLocations();
	int minDistance = std::numeric_limits<int>::max();
	const BaseLocation * closestBase;
	for (const auto & base : bases)
	{
		int distance=base->getGroundDistance(pos);
		if (minDistance > distance && base->isOccupiedByPlayer(Players::Self))
		{
			minDistance = distance;
			closestBase = base;
		}
	}
	return closestBase;
}

int Util::GetUnitTypeMineralPrice(const sc2::UnitTypeID type, const CCBot & bot)
{
    return bot.Observation()->GetUnitTypeData()[type].mineral_cost;
}

int Util::GetUnitTypeGasPrice(const sc2::UnitTypeID type, const CCBot & bot)
{
    return bot.Observation()->GetUnitTypeData()[type].vespene_cost;
}

int Util::GetUnitTypeWidth(const sc2::UnitTypeID type, const CCBot & bot)
{
    return (int)(2 * bot.Observation()->GetAbilityData()[bot.Data(type).buildAbility].footprint_radius);
}

int Util::GetUnitTypeHeight(const sc2::UnitTypeID type, const CCBot & bot)
{
    return (int)(2 * bot.Observation()->GetAbilityData()[bot.Data(type).buildAbility].footprint_radius);
}

float Util::GetUnitTypeRange(const sc2::UnitTypeID type, const CCBot & bot)
{
	//todo choose correct weapon
	std::vector<sc2::Weapon> weapon= bot.Observation()->GetUnitTypeData()[type].weapons;
	if (weapon.size()>0)
		return weapon[0].range;
	return 0.f;
}

const float Util::GetUnitTypeSight(const sc2::UnitTypeID type, const CCBot & bot)
{
	return bot.Observation()->GetUnitTypeData()[type].sight_range;
}

bool Util::UnitOutrangesMe(const sc2::UnitTypeID me, const sc2::UnitTypeID attacker, const CCBot & bot)
{
	return bot.Observation()->GetUnitTypeData()[me].weapons[0].range<= bot.Observation()->GetUnitTypeData()[attacker].weapons[0].range;
}

sc2::Point2D Util::CalcCenter(const std::vector<const sc2::Unit *> & units)
{
    if (units.empty())
    {
        return sc2::Point2D(0.0f,0.0f);
    }

    float cx = 0.0f;
    float cy = 0.0f;

    for (const auto & unit : units)
    {
        BOT_ASSERT(unit, "Unit pointer was null");
        cx += unit->pos.x;
        cy += unit->pos.y;
    }

    return sc2::Point2D(cx / units.size(), cy / units.size());
}

bool Util::IsDetector(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsDetectorType(unit->unit_type);
}

float Util::GetAttackRange(const sc2::UnitTypeID & type, CCBot & bot)
{
    auto & weapons = bot.Observation()->GetUnitTypeData()[type].weapons;
    
    if (weapons.empty())
    {
		if (type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
		{
			return 4.0f;
		}
        return 0.0f;
    }

    float maxRange = 0.0f;
    for (const auto & weapon : weapons)
    {
        if (weapon.range > maxRange)
        {
            maxRange = weapon.range;
        }
    }

    return maxRange;
}

bool Util::IsDetectorType(const sc2::UnitTypeID & type)
{
    switch (type.ToType())
    {
        case sc2::UNIT_TYPEID::PROTOSS_OBSERVER        : return true;
        case sc2::UNIT_TYPEID::ZERG_OVERSEER           : return true;
        case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET    : return true;
        case sc2::UNIT_TYPEID::ZERG_SPORECRAWLER       : return true;
        case sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON    : return true;
        case sc2::UNIT_TYPEID::TERRAN_RAVEN            : return true;
        default: return false;
    }
}

bool Util::IsBurrowedType(const sc2::UnitTypeID & type)
{
	switch (type.ToType())
	{
	case sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_BANELINGBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_HYDRALISKBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_INFESTORBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_LURKERMPBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_QUEENBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_ROACHBURROWED: return true;
	case sc2::UNIT_TYPEID::ZERG_SWARMHOSTBURROWEDMP: return true;
	case sc2::UNIT_TYPEID::ZERG_ZERGLINGBURROWED: return true;
	default: return false;
	}
}

int Util::GetPlayer(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    if (unit->alliance == sc2::Unit::Alliance::Self)
    {
        return 0;
    }

    if (unit->alliance == sc2::Unit::Alliance::Enemy)
    {
        return 1;
    }

    if (unit->alliance == sc2::Unit::Alliance::Neutral)
    {
        return 2;
    }

    return -1;
}

bool Util::IsCombatUnitType(const sc2::UnitTypeID & type, CCBot & bot)
{
    if (IsWorkerType(type)) { return false; }
    if (IsSupplyProviderType(type)) { return false; }
    if (bot.Data(type).isBuilding && (type.ToType() != sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON) && (type.ToType() != sc2::UNIT_TYPEID::TERRAN_MISSILETURRET) && (type.ToType() != sc2::UNIT_TYPEID::ZERG_SPINECRAWLER) && (type.ToType() != sc2::UNIT_TYPEID::ZERG_SPORECRAWLER)) { return false; }

    if (type == sc2::UNIT_TYPEID::ZERG_EGG) { return false; }
    if (type == sc2::UNIT_TYPEID::ZERG_LARVA) { return false; }
	if (type == sc2::UNIT_TYPEID::PROTOSS_ADEPTPHASESHIFT) { return false; }

    return true;
}

bool Util::IsCombatUnit(const sc2::Unit * unit, CCBot & bot)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsCombatUnitType(unit->unit_type, bot);
}

bool Util::IsSupplyProviderType(const sc2::UnitTypeID & type)
{
    switch (type.ToType()) 
    {
        case sc2::UNIT_TYPEID::ZERG_OVERLORD                : return true;
		case sc2::UNIT_TYPEID::ZERG_OVERLORDCOCOON			: return true;
		case sc2::UNIT_TYPEID::ZERG_OVERLORDTRANSPORT		: return true;
		case sc2::UNIT_TYPEID::ZERG_TRANSPORTOVERLORDCOCOON	: return true;
        case sc2::UNIT_TYPEID::PROTOSS_PYLON                : return true;
        case sc2::UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED     : return true;
        case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT           : return true;
        case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED    : return true;
        default: return false;
    }

    return true;
}

bool Util::IsSupplyProvider(const sc2::Unit * unit)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    return IsSupplyProviderType(unit->unit_type);
}

void Util::swapBuildings(const sc2::Unit * a, const sc2::Unit * b, CCBot & bot)
{
	if (!a->is_flying)
	{
		bot.Actions()->UnitCommand(a, sc2::ABILITY_ID::LIFT);
		bot.Actions()->UnitCommand(b, sc2::ABILITY_ID::LIFT);
	}
	else
	{
		const sc2::Point2D a_pos = a->pos;
		const sc2::Point2D b_pos = b->pos;
		bot.Actions()->UnitCommand(a, sc2::ABILITY_ID::LAND,b_pos);
		bot.Actions()->UnitCommand(b, sc2::ABILITY_ID::LAND,a_pos);
	}
	return;
}


float Util::Dist(const sc2::Point2D & p1, const sc2::Point2D & p2)
{
    return sqrtf(Util::DistSq(p1,p2));
}

float Util::Dist(const sc2::Point2D & p1)
{
	return sqrtf(Util::DistSq(p1));
}

float Util::DistSq(const sc2::Point2D & p1, const sc2::Point2D & p2)
{
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;

    return dx*dx + dy*dy;
}

float Util::DistSq(const sc2::Point2D & p1)
{
	float dx = p1.x;
	float dy = p1.y;

	return dx*dx + dy*dy;
}

bool Util::Pathable(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return false;
    }

    assert(info.pathing_grid.data.size() == info.width * info.height);
    unsigned char encodedPlacement = info.pathing_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    bool decodedPlacement = encodedPlacement == 255 ? false : true;
    return decodedPlacement;
}

bool Util::Placement(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return false;
    }

    assert(info.placement_grid.data.size() == info.width * info.height);
    unsigned char encodedPlacement = info.placement_grid.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    bool decodedPlacement = encodedPlacement == 255 ? true : false;
    return decodedPlacement;
}

float Util::TerainHeight(const sc2::GameInfo & info, const sc2::Point2D & point) 
{
    sc2::Point2DI pointI((int)point.x, (int)point.y);
    if (pointI.x < 0 || pointI.x >= info.width || pointI.y < 0 || pointI.y >= info.width)
    {
        return 0.0f;
    }

    assert(info.terrain_height.data.size() == info.width * info.height);
    unsigned char encodedHeight = info.terrain_height.data[pointI.x + ((info.height - 1) - pointI.y) * info.width];
    float decodedHeight = -100.0f + 200.0f * float(encodedHeight) / 255.0f;
    return decodedHeight;
}

void Util::VisualizeGrids(const sc2::ObservationInterface * obs, sc2::DebugInterface * debug) 
{
	if (!useDebug)
	{
		return;
	}
    const sc2::GameInfo& info = obs->GetGameInfo();

    sc2::Point2D camera = obs->GetCameraPos();
    for (float x = camera.x - 8.0f; x < camera.x + 8.0f; ++x) 
    {
        for (float y = camera.y - 8.0f; y < camera.y + 8.0f; ++y) 
        {
            // Draw in the center of each 1x1 cell
            sc2::Point2D point(x + 0.5f, y + 0.5f);

            float height = TerainHeight(info, sc2::Point2D(x, y));
            bool placable = Placement(info, sc2::Point2D(x, y));
            //bool pathable = Pathable(info, sc2::Point2D(x, y));

            sc2::Color color = placable ? sc2::Colors::Green : sc2::Colors::Red;
            debug->DebugSphereOut(sc2::Point3D(point.x, point.y, height + 0.5f), 0.4f, color);
        }
    }

}

std::string Util::GetStringFromRace(const sc2::Race & race)
{
    	switch ( race )
	{
		case sc2::Race::Protoss: return "Protoss";
		case sc2::Race::Terran:  return "Terran";
		case sc2::Race::Zerg:    return "Zerg";
		default: return "Random";
	}
}

sc2::Race Util::GetRaceFromString(const std::string & raceIn)
{
    std::string race(raceIn);
    std::transform(race.begin(), race.end(), race.begin(), ::tolower);

    if (race == "terran")
    {
        return sc2::Race::Terran;
    }
    else if (race == "protoss")
    {
        return sc2::Race::Protoss;
    }
    else if (race == "zerg")
    {
        return sc2::Race::Zerg;
    }
    else if (race == "random")
    {
        return sc2::Race::Random;
    }

    BOT_ASSERT(false, "Unknown Race: ", race.c_str());
    return sc2::Race::Terran;
}

sc2::UnitTypeID Util::GetUnitTypeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UnitTypeData & data : bot.Observation()->GetUnitTypeData())
    {
        if (name == data.name)
        {
            return data.unit_type_id;
        }
    }

    return 0;
}

sc2::UpgradeID Util::GetUpgradeIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::UpgradeData & data : bot.Observation()->GetUpgradeData())
    {
        if (name == data.name)
        {
            return data.upgrade_id;
        }
    }

    return 0;
}

sc2::BuffID Util::GetBuffIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::BuffData & data : bot.Observation()->GetBuffData())
    {
        if (name == data.name)
        {
            return data.buff_id;
        }
    }

    return 0;
}

sc2::AbilityID Util::GetAbilityIDFromName(const std::string & name, CCBot & bot)
{
    for (const sc2::AbilityData & data : bot.Observation()->GetAbilityData())
    {
        if (name == data.link_name)
        {
            return data.ability_id;
        }
    }

    return 0;
}


UnitTag GetClosestEnemyUnitTo(const sc2::Unit * ourUnit, const sc2::ObservationInterface * obs)
{
    UnitTag closestTag = 0;
	double closestDist = std::numeric_limits<double>::max();

	for (const auto & unit : obs->GetUnits())
	{
		double dist = Util::DistSq(unit->pos, ourUnit->pos);

		if (!closestTag || (dist < closestDist))
		{
			closestTag = unit->tag;
			closestDist = dist;
		}
	}

	return closestTag;
}

// checks where a given unit can make a given unit type now
// this is done by iterating its legal abilities for the build command to make the unit
bool Util::UnitCanBuildTypeNow(const sc2::Unit * unit, const sc2::UnitTypeID & type, CCBot & m_bot)
{
    BOT_ASSERT(unit, "Unit pointer was null");
    sc2::AvailableAbilities available_abilities = m_bot.Query()->GetAbilitiesForUnit(unit);
    
    // quick check if the unit can't do anything it certainly can't build the thing we want
    if (available_abilities.abilities.empty()) 
    {
        return false;
    }
    else 
    {
        // check to see if one of the unit's available abilities matches the build ability type
        sc2::AbilityID buildTypeAbility = m_bot.Data(type).buildAbility;
        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities) 
        {
            if (available_ability.ability_id == buildTypeAbility)
            {
                return true;
            }
        }
    }

    return false;
}

bool Util::canHitMe(const sc2::Unit * me, const sc2::Unit * hitter, CCBot & bot)
{
	if (hitter->build_progress != 1.0f)
	{
		return false;
	}
	if (me->unit_type.ToType() == sc2::UNIT_TYPEID::PROTOSS_COLOSSUS)
	{
		return true;
	}
	std::vector<sc2::Weapon> weapons = bot.Observation()->GetUnitTypeData()[hitter->unit_type].weapons;
	if (me->is_flying)
	{
		for (const auto & weapon : weapons)
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
		if (hitter->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_BANELING)
		{
			return true;
		}
		for (const auto & weapon : weapons)
		{
			if (weapon.type == sc2::Weapon::TargetType::Ground || weapon.type == sc2::Weapon::TargetType::Any)
			{
				return true;
			}
		}
	}
	return false;
}

const sc2::Unit * Util::getClostestMineral(sc2::Point2D pos, CCBot & bot)
{
	const std::set<const BaseLocation *> bases = bot.Bases().getOccupiedBaseLocations(Players::Self);
	std::map<int, const BaseLocation *> orderedBases;
	for (const auto & base : bases)
	{
		//We don't care for not finished bases.
		const sc2::Unit * townHall = base->getTownHall();
		if (townHall && townHall->build_progress != 1.0f && townHall->assigned_harvesters<24)
		{
			continue;
		}
		int distance = base->getGroundDistance(pos);
		orderedBases[distance] = base;
	}
	for (const auto & baseMap : orderedBases)
	{
		const std::vector<const sc2::Unit *> mineralPatches = baseMap.second->getMinerals();
		std::map<int, const sc2::Unit *> orderedMinerals;
		for (const auto & mineralPatch : mineralPatches)
		{
			int distance = bot.Map().getGroundDistance(mineralPatch->pos, pos);

			//Bad things happen if it is not alive or just snapshot
			if (mineralPatch->is_alive && mineralPatch->display_type == sc2::Unit::DisplayType::Visible && mineralPatch->mineral_contents > 200)
			{
				orderedMinerals[distance] = mineralPatch;
			}
			
		}
		for (const auto & mineralPatch : orderedMinerals )
		{
			return mineralPatch.second;
		}
	}
	//If we get till here its time to long distance mine
	const std::vector<const BaseLocation *> bases2 = bot.Bases().getBaseLocations();
	std::map<int, const BaseLocation *> orderedBases2;
	for (const auto & base : bases2)
	{
		if (base->isOccupiedByPlayer(Players::Self))
		{
			continue;
		}
		int distance = base->getGroundDistance(pos);
		orderedBases2[distance] = base;
	}
	for (const auto & baseMap : orderedBases2)
	{
		const std::vector<const sc2::Unit *> mineralPatches = baseMap.second->getMinerals();
		for (const auto & mineralPatch : mineralPatches)
		{
			//Bad things happen if it is not alive or just snapshot. But they are probably snapshots if we get till here.
			if (mineralPatch->is_alive && mineralPatch->display_type == sc2::Unit::DisplayType::Visible && mineralPatch->mineral_contents > 200)
			{
				return mineralPatch;
			}
		}
	}
	return nullptr;
}

std::vector<sc2::UNIT_TYPEID> Util::getMineralTypes()
{
	std::vector<sc2::UNIT_TYPEID> minerals = { sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_BATTLESTATIONMINERALFIELD750,
												sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_PURIFIERMINERALFIELD750,
												sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_PURIFIERRICHMINERALFIELD750,
												sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_LABMINERALFIELD750,
												sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750,
												sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD,
												sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750 };
	return minerals;
}

const sc2::UpgradeID Util::abilityIDToUpgradeID(const sc2::ABILITY_ID id)
{
	switch (id)
	{
	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL1: return sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1;
	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL2: return sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2;
	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL3: return sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3;

	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL1: return sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1;
	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL2: return sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2;
	case sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL3: return sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3;
	}
	return 0;
}


sc2::Point3D Util::get3DPoint(const sc2::Point2D pos,CCBot & bot)
{
	return sc2::Point3D(pos.x,pos.y,bot.Observation()->TerrainHeight(pos));
}

sc2::Point2D Util::normalizeVector(const sc2::Point2D pos, const float length)
{
	return (length / Util::Dist(pos)) * pos ;
}

const bool Util::isBadEffect(const sc2::EffectID id)
{
	if (id.ToType() == sc2::EFFECT_ID::LURKERATTACK)
	{
		int a = 1;
	}
	switch (id.ToType())
	{
	case sc2::EFFECT_ID::BLINDINGCLOUD:
	case sc2::EFFECT_ID::CORROSIVEBILE:
	case sc2::EFFECT_ID::LIBERATORMORPHED:
	case sc2::EFFECT_ID::LIBERATORMORPHING:
	case sc2::EFFECT_ID::LURKERATTACK:
	case sc2::EFFECT_ID::NUKEDOT:
	case sc2::EFFECT_ID::PSISTORM:
	case sc2::EFFECT_ID::THERMALLANCES:
		return true;
	}
	return false;
}