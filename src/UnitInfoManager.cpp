#include "UnitInfoManager.h"
#include "Util.h"
#include "CCBot.h"

#include <sstream>


UnitInfoManager::UnitInfoManager(CCBot & bot)
    : m_bot(bot)
{

}

void UnitInfoManager::onStart()
{

}

void UnitInfoManager::onFrame()
{
    updateUnitInfo();
    drawUnitInformation(100, 100);
    drawSelectedUnitDebugInfo();
}

void UnitInfoManager::updateUnitInfo()
{
    m_units[Players::Self].clear();
    m_units[Players::Enemy].clear();

	//DT detection
	const std::vector<sc2::UpgradeID> upgrades = m_bot.Observation()->GetUpgrades();
	int armor = 0;
	if (std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3) != upgrades.end())
	{
		armor = 3;
	}
	else if (std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2) != upgrades.end())
	{
		armor = 2;
	}
	else if (std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1) != upgrades.end())
	{
		armor = 1;
	}
	if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss && m_unitData.size() > 1)
	{
		for (const auto & kv : getUnitData(Players::Self).getUnitInfoMap())
		{
			const float lostHealth = kv.second.lastHealth - kv.first->health;
			if (lostHealth == 45.0f-armor || lostHealth == 50.0f - armor || lostHealth == 55.0f - armor || lostHealth == 60.0f - armor)
			{
				m_bot.OnDTdetected(kv.first->pos);
			}
		}
	}

	for (auto & unit : m_bot.Observation()->GetUnits())
	{
		if (Util::GetPlayer(unit) == Players::Self || Util::GetPlayer(unit) == Players::Enemy)
		{
			updateUnit(unit);
			m_units[Util::GetPlayer(unit)].push_back(unit);
		}
	}

	// Update the location of units we can not see now and the last seen position is visible
	if (m_unitData.size() > 1)
	{
		for (auto& kv : getUnitData(Players::Enemy).getUnitInfoMap())
		{
			if (kv.first->last_seen_game_loop!=m_bot.Observation()->GetGameLoop() && !Util::IsBuildingType(kv.second.type, m_bot) && !Util::IsBurrowedType(kv.second.type) && m_bot.Observation()->GetVisibility(kv.second.lastPosition) == sc2::Visibility::Visible)
			{
				m_unitData[Players::Enemy].lostPosition(kv.first);
			}
		}
	}

	// remove bad enemy units
    m_unitData[Players::Self].removeBadUnits();
    m_unitData[Players::Enemy].removeBadUnits();
}

const std::map<const sc2::Unit *, UnitInfo> & UnitInfoManager::getUnitInfoMap(int player) const
{
    return getUnitData(player).getUnitInfoMap();
}

const std::vector<const sc2::Unit *> & UnitInfoManager::getUnits(int player) const
{
    BOT_ASSERT(m_units.find(player) != m_units.end(), "Couldn't find player units: %d", player);

    return m_units.at(player);
}
const int UnitInfoManager::getNumCombatUnits(int player) const
{
	BOT_ASSERT(m_units.find(player) != m_units.end(), "Couldn't find player units: %d", player);

	int numCombatUnits = 0;
	for (auto& kv : getUnitData(player).getUnitInfoMap())
	{
		if (Util::IsCombatUnit(kv.first,m_bot))
		{
			numCombatUnits++;
		}
	}
	return numCombatUnits;
}
const std::vector<const sc2::Unit *> UnitInfoManager::getBuildings(int player) const
{
	BOT_ASSERT(m_units.find(player) != m_units.end(), "Couldn't find player units: %d", player);
	std::vector<const sc2::Unit *> buildings;
	for (auto & unit : m_units.at(player))
	{
		if (m_bot.Data(unit->unit_type).isBuilding)
		{
			buildings.push_back(unit);
		}
	}
	return buildings;
}

static std::string GetAbilityText(sc2::AbilityID ability_id) {
    std::string str;
    str += sc2::AbilityTypeToName(ability_id);
    str += " (";
    str += std::to_string(uint32_t(ability_id));
    str += ")";
    return str;
}

void UnitInfoManager::drawSelectedUnitDebugInfo()
{
	if (!useDebug)
	{
		return;
	}
    const sc2::Unit * unit = nullptr;
    for (auto u : m_bot.Observation()->GetUnits()) 
    {
        if (u->is_selected && u->alliance == sc2::Unit::Self) {
            unit = u;
            break;
        }
    }

    if (!unit) { return; }

    auto query = m_bot.Query();
    auto abilities = m_bot.Observation()->GetAbilityData();

    std::string debug_txt;
    debug_txt = UnitTypeToName(unit->unit_type);
    if (debug_txt.length() < 1) 
    {
        debug_txt = "(Unknown name)";
        assert(0);
    }
    debug_txt += " (" + std::to_string(unit->unit_type) + ")";
        
    sc2::AvailableAbilities available_abilities = query->GetAbilitiesForUnit(unit);
    if (available_abilities.abilities.size() < 1) 
    {
        std::cout << "No abilities available for this unit: "<< unit->unit_type << std::endl;
    }
    else 
    {
        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities) 
        {
            if (available_ability.ability_id >= abilities.size()) { continue; }

            const sc2::AbilityData & ability = abilities[available_ability.ability_id];

            debug_txt += GetAbilityText(ability.ability_id) + "\n";
        }
    }
    Drawing::drawTextScreen(m_bot, unit->pos, debug_txt, sc2::Colors::Green);

    // Show the direction of the unit.
    sc2::Point3D p1; // Use this to show target distance.
    {
        const float length = 5.0f;
        sc2::Point3D p0 = unit->pos;
        p0.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
        p1 = unit->pos;
        assert(unit->facing >= 0.0f && unit->facing < 6.29f);
        p1.x += length * std::cos(unit->facing);
        p1.y += length * std::sin(unit->facing);
		Drawing::drawLine(m_bot,p0, p1, sc2::Colors::Yellow);
    }

    // Box around the unit.
    {
        sc2::Point3D p_min = unit->pos;
        p_min.x -= 2.0f;
        p_min.y -= 2.0f;
        p_min.z -= 2.0f;
        sc2::Point3D p_max = unit->pos;
        p_max.x += 2.0f;
        p_max.y += 2.0f;
        p_max.z += 2.0f;
		Drawing::drawBox(m_bot,p_min, p_max, sc2::Colors::Blue);
    }

    // Sphere around the unit.
    {
        sc2::Point3D p = unit->pos;
        p.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
		Drawing::drawSphere(m_bot,p, 1.25f, sc2::Colors::Purple);
    }

    // Pathing query to get the target.
    bool has_target = false;
    sc2::Point3D target;
    std::string target_info;
    for (const sc2::UnitOrder& unit_order : unit->orders)
    {
        // TODO: Need to determine if there is a target point, no target point, or the target is a unit/snapshot.
        target.x = unit_order.target_pos.x;
        target.y = unit_order.target_pos.y;
        target.z = p1.z;
        has_target = true;

        target_info = "Target:\n";
        if (unit_order.target_unit_tag != 0x0LL) {
            target_info += "Tag: " + std::to_string(unit_order.target_unit_tag) + "\n";
        }
        if (unit_order.progress != 0.0f && unit_order.progress != 1.0f) {
            target_info += "Progress: " + std::to_string(unit_order.progress) + "\n";
        }

        // Perform the pathing query.
        {
            float distance = query->PathingDistance(unit->pos, target);
            target_info += "\nPathing dist: " + std::to_string(distance);
        }

        break;
    }

    if (has_target)
    {
        sc2::Point3D p = target;
        p.z += 0.1f; // Raise the line off the ground a bit so it renders more clearly.
		Drawing::drawSphere(m_bot,target, 1.25f, sc2::Colors::Blue);
		Drawing::drawTextScreen(m_bot, p1, target_info, sc2::Colors::Yellow);
    }
    
}

// passing in a unit type of 0 returns a count of all units
size_t UnitInfoManager::getUnitTypeCount(int player, sc2::UnitTypeID type, bool completed) const
{
    size_t count = 0;

    for (auto & unit : getUnits(player))
    {
        if ((!type || type == unit->unit_type) && (!completed || unit->build_progress == 1.0f))
        {
            count++;
        }
    }

    return count;
}

void UnitInfoManager::drawUnitInformation(float x,float y) const
{
	if (!useDebug)
	{
		return;
	}
    if (!m_bot.Config().DrawEnemyUnitInfo)
    {
        //return;
    }

    std::stringstream ss;

    // for each unit in the queue
    for (int t(0); t < 255; t++)
    {
        int numUnits =      m_unitData.at(Players::Self).getNumUnits(t);
        int numDeadUnits =  m_unitData.at(Players::Enemy).getNumDeadUnits(t);

        // if there exist units in the vector
        if (numUnits > 0)
        {
            ss << numUnits << "   " << numDeadUnits << "   " << sc2::UnitTypeToName(t) << "\n";
        }
    }
    
    for (auto & kv : getUnitData(Players::Enemy).getUnitInfoMap())
    {
        Drawing::drawSphere(m_bot,kv.second.lastPosition, 0.5f);
        Drawing::drawTextScreen(m_bot, kv.second.lastPosition,sc2::UnitTypeToName(kv.second.type));
    }


}

void UnitInfoManager::updateUnit(const sc2::Unit * unit)
{
    if (!(Util::GetPlayer(unit) == Players::Self || Util::GetPlayer(unit) == Players::Enemy))
    {
        return;
    }
	//KD8 charges are not really our units
	if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		return;
	}
	//If it is not just a snap shot
	if (unit->display_type != sc2::Unit::DisplayType::Snapshot)
	{
		if (unit->is_alive)
		{
			m_unitData[Util::GetPlayer(unit)].updateUnit(unit);
		}
		else
		{
			m_unitData[Util::GetPlayer(unit)].killUnit(unit);
		}
	}
	//else if we see the position and still get only a snapshot
	else if (m_bot.Observation()->GetVisibility(unit->pos) == sc2::Visibility::Visible)
	{
		//and there should be a building(actually only buildings should be here), but there is not, it seems to be dead
		if (Util::IsBuildingType(unit->unit_type, m_bot))
		{
			if (!m_bot.GetUnit(unit->tag))
			{
				m_unitData[Util::GetPlayer(unit)].killUnit(unit);
			}
		}
	}
}

// is the unit valid?
bool UnitInfoManager::isValidUnit(const sc2::Unit * unit)
{
    if (!unit) { return false; }

    // we only care about our units and enemy units
    if (!(Util::GetPlayer(unit) == Players::Self || Util::GetPlayer(unit) == Players::Enemy))
    {
        return false;
    }

    // if it's a weird unit, don't bother
    if (unit->unit_type == sc2::UNIT_TYPEID::ZERG_EGG || unit->unit_type == sc2::UNIT_TYPEID::ZERG_LARVA)
    {
        return false;
    }

    // if the position isn't valid throw it out
    if (!m_bot.Map().isValid(unit->pos))
    {
        return false;
    }
    
    // s'all good baby baby
    return true;
}

void UnitInfoManager::getNearbyForce(std::vector<UnitInfo> & unitInfo, sc2::Point2D p, int player, float radius) const
{
    bool hasBunker = false;
    // for each unit we know about for that player
    for (const auto & kv : getUnitData(player).getUnitInfoMap())
    {
        const UnitInfo & ui(kv.second);

        // if it's a combat unit we care about
        // and it's finished! 
        if (Util::IsCombatUnitType(ui.type, m_bot) && Util::Dist(ui.lastPosition,p) <= radius)
        {
            // add it to the vector
            unitInfo.push_back(ui);
        }
    }
}

const UnitData & UnitInfoManager::getUnitData(int player) const
{
    return m_unitData.find(player)->second;
}
