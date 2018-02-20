#include "UnitInfoManager.h"
#include "Util.h"
#include "CCBot.h"
#include "CUnit.h"

#include <sstream>


UnitInfoManager::UnitInfoManager(CCBot & bot)
    : m_bot(bot)
{

}

void UnitInfoManager::onStart()
{
	updateUnitInfo();
}

void UnitInfoManager::onFrame()
{
    updateUnitInfo();
}

void UnitInfoManager::updateUnitInfo()
{
	for (const auto & unit : m_bot.Observation()->GetUnits())
	{
		m_unitDataBase[Util::GetPlayer(unit)][unit->unit_type].insert(unit, m_bot);
	}
	for (auto & playerData : m_unitDataBase)
	{
		for (auto & units : playerData.second)
		{
			units.second.removeDead();
		}
	}
}

const size_t UnitInfoManager::getNumCombatUnits(int player) const
{
	if (m_unitDataBase.find(player) == m_unitDataBase.end())
	{
		return 0;
	}
	size_t numCombatUnits = 0;

	for (const auto & units : m_unitDataBase.at(player))
		{
		if (Util::IsCombatUnitType(units.first,m_bot))
		{
			numCombatUnits += units.second.getUnits().size();
		}
	}
	return numCombatUnits;
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

const size_t UnitInfoManager::getUnitTypeCount(int player, sc2::UnitTypeID type, bool completed) const
{
	if (!type || m_unitDataBase.find(player) == m_unitDataBase.end() || m_unitDataBase.at(player).find(type)== m_unitDataBase.at(player).end())
	{
		return 0;
	}
    size_t count = 0;
	if (!completed)
	{
		return m_unitDataBase.at(player).at(type).getUnits().size();
	}
    for (const auto & unit : m_unitDataBase.at(player).at(type).getUnits())
    {
        if (unit->isCompleted())
        {
            count++;
        }
    }
    return count;
}

const size_t UnitInfoManager::getUnitTypeCount(int player, std::vector<sc2::UnitTypeID> types, bool completed) const
{
	size_t count = 0;
	for (const auto & type : types)
	{
		count += getUnitTypeCount(player, type, completed);
	}
	return count;
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
const CUnit_ptr UnitInfoManager::OnUnitCreate(const sc2::Unit * unit)
{
	return m_unitDataBase[Util::GetPlayer(unit)][unit->unit_type].insert(unit, m_bot);
}

const std::shared_ptr<CUnit> UnitInfoManager::getUnit(sc2::Tag unitTag)
{
	const sc2::Unit * unit = m_bot.Observation()->GetUnit(unitTag);
	if (unit)
	{
		int player = unit->owner;
		sc2::UnitTypeID type = unit->unit_type;
		if (m_unitDataBase.find(player) == m_unitDataBase.end() || m_unitDataBase.at(player).find(type) == m_unitDataBase.at(player).end())
		{
			return m_unitDataBase[Util::GetPlayer(unit)][unit->unit_type].insert(unit, m_bot);
		}
		for (const auto & cunit : m_unitDataBase.at(player).at(type).getUnits())
		{
			if (cunit->getTag() == unitTag)
			{
				return cunit;
			}
		}
	}
	else
	{
		for (auto & playerData : m_unitDataBase)
		{
			for (auto & unitData : playerData.second)
			{
				for (auto & cunit : unitData.second.getUnits())
				{
					if (cunit->getTag() == unitTag)
					{
						return cunit;
					}
				}
			}
		}
	}
	return nullptr;
}

const std::shared_ptr<CUnit> UnitInfoManager::getUnit(const sc2::Unit * unit)
{
	if (unit)
	{
		int player = unit->owner;
		sc2::UnitTypeID type = unit->unit_type;
		if (m_unitDataBase.find(player) == m_unitDataBase.end() || m_unitDataBase.at(player).find(type) == m_unitDataBase.at(player).end())
		{
			m_unitDataBase[Util::GetPlayer(unit)][unit->unit_type].insert(unit, m_bot);
		}
		for (const auto & cunit : m_unitDataBase.at(player).at(type).getUnits())
		{
			if (cunit->getTag() == unit->tag)
			{
				return cunit;
			}
		}
	}
	return nullptr;
}

const std::vector<std::shared_ptr<CUnit>> UnitInfoManager::getUnits(int player) const
{
	std::vector<std::shared_ptr<CUnit>> allUnits;
	if (m_unitDataBase.find(player) == m_unitDataBase.end())
	{
		return allUnits;
	}
	for (const auto & u : m_unitDataBase.at(player))
	{
		std::vector<std::shared_ptr<CUnit>> newUnits(u.second.getUnits());
		//Too cowardly to do std::make_move_iterator
		allUnits.insert(allUnits.end(),newUnits.begin(),newUnits.end());
	}
	return allUnits;
}

const std::vector<std::shared_ptr<CUnit>> UnitInfoManager::getUnits(int player, sc2::UnitTypeID type) const
{
	std::vector<std::shared_ptr<CUnit>> allUnits;
	if (m_unitDataBase.find(player) == m_unitDataBase.end() || m_unitDataBase.at(player).find(type)== m_unitDataBase.at(player).end())
	{
		return allUnits;
	}
	return m_unitDataBase.at(player).at(type).getUnits();
}

const std::vector<std::shared_ptr<CUnit>> UnitInfoManager::getUnits(int player, std::vector<sc2::UnitTypeID> types) const
{
	std::vector<std::shared_ptr<CUnit>> allUnits;
	if (m_unitDataBase.find(player) == m_unitDataBase.end())
	{
		return allUnits;
	}
	for (const auto & type : types)
	{
		if (m_unitDataBase.at(player).find(type) == m_unitDataBase.at(player).end())
		{
			continue;
		}
		std::vector<std::shared_ptr<CUnit>> newUnits(m_unitDataBase.at(player).at(type).getUnits());
		//Too cowardly to do std::make_move_iterator
		allUnits.insert(allUnits.end(), newUnits.begin(), newUnits.end());
	}
	return allUnits;
}

const std::vector<std::shared_ptr<CUnit>> UnitInfoManager::getUnits(int player, std::vector<sc2::UNIT_TYPEID> types) const
{
	std::vector<std::shared_ptr<CUnit>> allUnits;
	if (m_unitDataBase.find(player) == m_unitDataBase.end())
	{
		return allUnits;
	}
	for (const auto & type : types)
	{
		if (m_unitDataBase.at(player).find(type) == m_unitDataBase.at(player).end())
		{
			continue;
		}
		std::vector<std::shared_ptr<CUnit>> newUnits(m_unitDataBase.at(player).at(type).getUnits());
		//Too cowardly to do std::make_move_iterator
		allUnits.insert(allUnits.end(), newUnits.begin(), newUnits.end());
	}
	return allUnits;
}