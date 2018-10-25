#include "ProductionManager.h"
#include "Util.h"
#include "CCBot.h"
#include "Micro.h"
#include "sc2api/sc2_score.h"




const int UNIT = 5;
const int BUILDING = 3;


ProductionManager::ProductionManager(CCBot & bot)
	: m_bot(bot)
	, m_buildingManager(bot)
	, m_scoutRequested(false)
	, m_vikingRequested(false)
	, m_liberatorRequested(false)
	, m_scansRequested(0)
	, m_defaultMacroSleep(0)
    , m_defaultMacroSleepMax(10)
	, m_turretsRequested(false)
	, m_needCC(false)
{
}

void ProductionManager::onStart()
{
	m_buildingManager.onStart();
	if (m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Zerg)
	{
		m_liberatorRequested = true;
	}
}

void ProductionManager::onFrame()
{

    // This avoids repeating the same command twice.
    ++m_defaultMacroSleep;
    if (m_defaultMacroSleep >= m_defaultMacroSleepMax)
    {
        macroMechanic();
        defaultMacroBio();
    }

	detectBuildOrderDeadlock();
	// ADD THINGS TO THE QUEUE BEFORE THIS POINT! IT WILL BE TAKEN FROM THE QUEUE BUT ONLY APPLIED AT THE END OF THE FRAME!!

	// check the _queue for stuff we can build
	manageBuildOrderQueue();

	m_buildingManager.onFrame();
	drawProductionInformation();
}

// on unit destroy
void ProductionManager::onUnitDestroy(const sc2::Unit * unit)
{
	m_buildingManager.onBuildingDestroyed(unit);
}

void ProductionManager::manageBuildOrderQueue()
{
	if (m_newQueue.empty())
	{
		return;
	}
	while (!m_newQueue.empty())
	{
		create(m_newQueue.front());
		m_newQueue.pop_front();
	}
}


// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::create(BuildOrderItem item)
{
	// send the building task to the building manager
	if (Util::IsTownHallType(item.m_type.getUnitTypeID()))
	{
		m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getNextExpansion(Players::Self));
	}
	// We want to switch addons from factory to starport. So better build them close.
	else if (item.m_type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORT)
	{
		const CUnits Factories = m_bot.UnitInfo().getUnits(sc2::Unit::Alliance::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_FACTORY , sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
		if (Factories.size() > 0)
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), Factories.front()->getPos());
		}
		else
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getBuildingLocation());
		}
	}
	else if (item.m_type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
	{
		for (const auto & base : m_bot.Bases().getOccupiedBaseLocations(Players::Self))
		{
			if (!base->getTownHall())
			{
				continue;
			}
			bool protectedBase = false;
			for (const auto & mt : m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET))
			{
				if (Util::DistSq(base->getCenterOfMinerals(), mt->getPos()) < 225.0f)
				{
					protectedBase = true;
					break;
				}
			}
			if (!protectedBase)
			{
				m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), base->getCenterOfMinerals());
				return;
			}
		}
		for (const auto & bunker : m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER))
		{
			bool protectedBunker = false;
			for (const auto & mt : m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET))
			{
				if (Util::DistSq(bunker->getPos(), mt->getPos()) < 25.0f)
				{
					protectedBunker = true;
					break;
				}
			}
			if (!protectedBunker)
			{
				m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), bunker->getPos());
				return;
			}
		}
	}
	else if (item.m_type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_BUNKER)
	{
		sc2::Point2D testPoint = m_bot.Map().getBunkerPosition();
		if (testPoint != sc2::Point2D(0, 0))
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), testPoint);
		}
		else
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getBuildingLocation());
		}
	}
	else if (m_bot.Map().hasPocketBase() && item.m_type.getUnitTypeID() == sc2::UNIT_TYPEID::TERRAN_BARRACKS && m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 2)
	{
		if (m_bot.Observation()->GetGameInfo().map_name == "Redshift LE")
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getNaturalExpansion(Players::Self)->getCenterOfBase());
		}
		else
		{
			m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getNaturalExpansion(Players::Self)->getCenterOfMinerals());
		}
	}
	else
	{
		m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getBuildingLocation());
	}
}

bool ProductionManager::detectBuildOrderDeadlock()
{
	return false;
}

size_t ProductionManager::getFreeMinerals()
{
	int reserved = m_buildingManager.getReservedMinerals();
	if (reserved > 1000 || reserved < 0)
	{
		m_buildingManager.resetFreeMinerals();
		reserved = 0;
	}
	return m_bot.Observation()->GetMinerals() - m_buildingManager.getReservedMinerals();
}

size_t ProductionManager::getFreeGas()
{
	return m_bot.Observation()->GetVespene() - m_buildingManager.getReservedGas();
}

void ProductionManager::macroMechanic()
{
    const CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes());
    size_t scansAvailable = 0;
    for (const auto & unit : CommandCenters)
    {
        if (unit->isCompleted() && !unit->isFlying())
        {
            if (unit->getEnergy() >= 50)
            {
                scansAvailable+=static_cast<size_t>(std::floor(unit->getEnergy()/50.0f));
                if (scansAvailable > m_scansRequested)
                {
                    const CUnit_ptr mineralPatch = Util::getClostestMineral(unit->getPos(), m_bot);
                    if (mineralPatch && mineralPatch->getDisplayType() == sc2::Unit::DisplayType::Visible)
                    {
                        Micro::SmartAbility(unit, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, mineralPatch, m_bot);
                    }
                }
            }
            // Sometimes we have a problem here
            if (unit->getAssignedHarvesters() == 0 || unit->getEnergy() > 60)
            {
                m_bot.Bases().assignTownhallToBase(unit);
            }
        }
    }
}

void ProductionManager::defaultMacroBio()
{
	m_defaultMacroSleep = 0;
    const size_t minerals = getFreeMinerals();
    // no money
	if (minerals < 50)
	{
		m_defaultMacroSleep = m_defaultMacroSleepMax;
		return;
	}
    size_t gas = getFreeGas();
	// for every production building we need 2 supply free
    const size_t supply = static_cast<size_t>(m_bot.Observation()->GetFoodUsed());
    const size_t supplyCap = static_cast<size_t>(m_bot.Observation()->GetFoodCap());

	// Maybe it would be smarter to search for all buildings first and then search through the resulting vector
	const CUnits Depots = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT , sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED }));
    const size_t numDepots = Depots.size();
    const size_t numDepotsFinished = buildingsFinished(Depots);

    const CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes());
    const size_t numBases = CommandCenters.size();
    const size_t numBasesFinished = buildingsFinished(CommandCenters);

	const CUnits Rax = m_bot.UnitInfo().getUnits(Players::Self,  sc2::UNIT_TYPEID::TERRAN_BARRACKS);
    const size_t numRax = Rax.size();
    const size_t numRaxFinished = buildingsFinished(Rax);

	const CUnits Starports = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_STARPORT , sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING }));
    const size_t numStarport = Starports.size();
    const size_t numStarportFinished = buildingsFinished(Starports);

	const CUnits Factories = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_FACTORY , sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
    const size_t numFactory = Factories.size();
    const size_t numFactoryFinished = buildingsFinished(Factories);

	const CUnits Engibays = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY);
    const size_t numEngibays = Engibays.size();
    const size_t numEngibaysFinished = buildingsFinished(Engibays);

    const CUnits bReactor = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR);
    const size_t numBReactor = bReactor.size();
    const size_t numBReactorFinished = buildingsFinished(bReactor);

    const CUnits bTechLab = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB);
    const size_t numBTechLab = bTechLab.size();
    const size_t numBTechLabFinished = buildingsFinished(bTechLab);

    if (m_bot.underAttack())
	{
		// If we are under attack marines have priority
        // Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Under attack!" << std::endl;
		for (const auto & unit : Rax)
		{
			// Any finished rax
			if (unit->isCompleted())
			{
				// that is idle
                const auto addon = unit->getAddOn();
                if (unit->isIdle() || (addon && addon->isType(sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR) && unit->getOrders().size() < 2))
				{
                    if (minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).mineralCost && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).supplyCost)
					{
						m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_MARINE);
                        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Marine" << std::endl;
						return;
					}
				}
			}
		}
		const CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
		if (m_bot.Workers().getMineralWorkers().size() > 5 && Bunker.size() + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BUNKER) < 1)
		{
			m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BUNKER), BUILDING, false));
            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Bunker" << std::endl;
			return;
		}
	}
	const float mineralRate = m_bot.Observation()->GetScore().score_details.collection_rate_minerals;
	const int reactorsFinished = buildingsFinished(m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR));
	if (!m_bot.underAttack() && (m_needCC || mineralRate / static_cast<float>(reactorsFinished) < 350.f) && numBases - numBasesFinished + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) == 0 && m_bot.Bases().getNextExpansion(Players::Self) != sc2::Point2D{})
	{
		if (std::find_if(CommandCenters.begin(), CommandCenters.end(), [](const auto & cc) { return cc->getAssignedHarvesters() < cc->getIdealHarvesters(); }) == CommandCenters.end())
		{
			if (minerals > 400)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER), BUILDING, false));
				m_needCC = false;
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "CC" << std::endl;
			}
			else
			{
				m_needCC = true;
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Need CC" << std::endl;
			}
			return;
		}
	}

	if (supplyCap < 200 && (supplyCap - supply < (numBasesFinished + 2 * numRaxFinished) || (numDepots == 0 && minerals >= 50 && m_bot.Observation()->GetFoodWorkers() == 13)))
	{
		// count the building and requested
		const int numBuildingDepots = numDepots-numDepotsFinished;
		if (numBuildingDepots + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < numBasesFinished)
		{
			if (minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT).mineralCost || (numDepots == 0 && minerals >= 50 && m_bot.Observation()->GetFoodWorkers() == 13))
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT), BUILDING, false));
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Depot" << std::endl;
			}
			else
			{
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving minerals for depot" << std::endl;
			}
			return;
		}
	}
	for (const auto & unit : CommandCenters)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			if (!unit->isFlying())
			{
				// Before building a worker, make sure it is a OC
				if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER && numRaxFinished > 0)
				{
					const BaseLocation * targetBase = m_bot.Bases().getBaseLocation(unit->getPos());
					if (targetBase && Util::DistSq(unit->getPos(), targetBase->getCenterOfBase()) > 0.01f && m_bot.Query()->Placement(sc2::ABILITY_ID::LAND_COMMANDCENTER, targetBase->getCenterOfBase()))
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::LIFT, m_bot);
						return;
					}
					if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND) < 2 || numEngibaysFinished == 0 || m_bot.GetPlayerRace(Players::Enemy) != sc2::Race::Zerg)
					{
						if (minerals >= 150)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "OC" << std::endl;
						}
						else
						{
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving minerals for OC" << std::endl;
						}
						return;
					}
					else
					{
						if (gas >= 150)
						{
							if (minerals >= 150)
							{
								Micro::SmartAbility(unit, sc2::ABILITY_ID::MORPH_PLANETARYFORTRESS, m_bot);
                                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "PF" << std::endl;
							}
							else
							{
                                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving minerals for PF" << std::endl;
							}
							return;
						}
						continue;
					}
				}

				// Every Base has to build a worker until 66 workers.
				if (m_bot.Observation()->GetFoodWorkers() < 66 && (unit->getIdealHarvesters() == 0 || unit->getAssignedHarvesters() <= unit->getIdealHarvesters() + 3 || numBases - numBasesFinished + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) == 1))
				{
					if (supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_SCV).supplyCost)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_SCV, m_bot);
                        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "TrainSCV" << std::endl;
						return;
					}
				}
			}
			else
			{
				const BaseLocation * targetBase = m_bot.Bases().getBaseLocation(unit->getPos());
				if (Util::DistSq(unit->getPos(), targetBase->getCenterOfBase()) > 0.01f && m_bot.Query()->Placement(sc2::ABILITY_ID::LAND_COMMANDCENTER, targetBase->getCenterOfBase()))
				{
					Micro::SmartAbility(unit, sc2::ABILITY_ID::LAND, targetBase->getCenterOfBase(), m_bot);
					return;
				}
			}
		}
	}
	// turrets
	if (m_turretsRequested && numEngibaysFinished > 0 && minerals < 300)
	{
		if (m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() + m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER) > m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_MISSILETURRET, false) + static_cast<size_t>(howOftenQueued(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)))
		{
			if (howOftenQueued(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET) == 0)
			{
				if (minerals >= 100)
				{
					m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET), BUILDING, false));
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "MISSILETURRET " << std::endl;
				}
				else
				{
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving for MISSILETURRET " << std::endl;
				}
				return;
			}
		}
	}
	// Refinaries
	int numOfRefinaries = static_cast<int>(m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_REFINERY, false) + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_REFINERY));
	if (gas <= 500 && numRax > 0 && minerals >= 75 && ((m_bot.Observation()->GetFoodWorkers() >= 15 && numOfRefinaries == 0) || (m_bot.Observation()->GetFoodWorkers() >= 21 && !m_bot.underAttack() && numOfRefinaries == 1) || (m_bot.Observation()->GetFoodWorkers() >= 40 && numOfRefinaries <= 3)))
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_REFINERY), BUILDING, false));
        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Refinary" << std::endl;
		return;
	}
    // Upgrades techlab
	// std::vector<sc2::UpgradeID> upgrades = m_bot.Observation()->GetUpgrades();
	for (const auto & unit : bTechLab)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			std::vector<sc2::AvailableAbility> abilities = m_bot.Query()->GetAbilitiesForUnit(unit->getUnit_ptr(), true).abilities;
			if (std::find_if(abilities.begin(), abilities.end(), [](const auto & ability) { return ability.ability_id == sc2::ABILITY_ID::RESEARCH_COMBATSHIELD; }) != abilities.end())
			{
				// If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_COMBATSHIELD, m_bot);
					}
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "CombatShield" << std::endl;
					return;
				}
			}
			else if (std::find_if(abilities.begin(), abilities.end(), [](const auto & ability) { return ability.ability_id == sc2::ABILITY_ID::RESEARCH_STIMPACK; }) != abilities.end())
			{
				// If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_STIMPACK, m_bot);
					}
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Stim" << std::endl;
					return;
				}
			}
			else if (std::find_if(abilities.begin(), abilities.end(), [](const auto & ability) { return ability.ability_id == sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS; }) != abilities.end())
			{
				// If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 50)
				{
					if (minerals >= 50)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS, m_bot);
					}
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Concussiv" << std::endl;
					return;
				}
			}
		}
	}

	// upgrades
	const CUnits Armories = m_bot.UnitInfo().getUnits(sc2::Unit::Alliance::Self, sc2::UNIT_TYPEID::TERRAN_ARMORY);
	const int numArmories = static_cast<int>(Armories.size());
    //const int numArmoriesFinished = buildingsFinished(Armories);

	const int weaponBio = m_bot.getWeaponBio();
	const int armorBio = m_bot.getArmorBio();
	for (const auto & unit : Engibays)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			std::vector<sc2::AvailableAbility> abilities = m_bot.Query()->GetAbilitiesForUnit(unit->getUnit_ptr(), true).abilities;
			// Weapons and armor research has consecutive numbers
			// First weapons
			if (std::find_if(abilities.begin(), abilities.end(), [](const auto & ability) { return ability.ability_id == sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS; }) != abilities.end())
			{
				if ((weaponBio == 0 && gas >= 100) || (weaponBio == 1 && gas >= 175) || (weaponBio == 2 && gas >= 250))
				{
					if ((weaponBio == 0 && minerals >= 100) || (weaponBio == 1 && minerals >= 175) || (weaponBio == 2 && minerals >= 250))
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS, m_bot);
					}
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Weapon ++" << std::endl;
					return;
				}
				else
				{
					// Block gas for upgrades
					gas = 0;
				}
			}
			// Then armor
			if (std::find_if(abilities.begin(), abilities.end(), [](const auto & ability) { return ability.ability_id == sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR; }) != abilities.end())
			{
				if ((armorBio == 0 && gas >= 100) || (armorBio == 1 && gas >= 175) || (armorBio == 2 && gas >= 250))
				{
					if ((armorBio == 0 && minerals >= 100) || (armorBio == 1 && minerals >= 175) || (armorBio == 2 && minerals >= 250))
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR, m_bot);
					}
                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Armor ++" << std::endl;
					return;
				}
				else
				{
					// Block gas for upgrades
					gas = 0;
				}
			}
		}
	}

	// Armory
	// Needed for upgrades after +1
	if (numArmories + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ARMORY) == 0 && numEngibays == 2)
	{
		// A armory needs 1040 frames to build.
		// +1 weapons needs 2560 frames.
		// So only after (2560-1040)/2560 seconds we need to build the armory
		// Since it always takes a while we do it 100 frames earlier
		float maxProgress = 0.0f;
		for (const auto & engi : Engibays)
		{
			if (engi->getOrders().empty())
			{
				maxProgress = std::min<float>(1.0f, m_bot.getWeaponBio());
				break;
			}
			const float progress = engi->getOrders().front().progress;
			if (maxProgress < progress)
			{
				maxProgress = progress;
			}
		}
		if (maxProgress >= 1420.0f / 2560.0f)
		{
			if (gas >= 100)
			{
				if (minerals >= 150)
				{
					m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_ARMORY), BUILDING, false));
				}
				return;
			}
			else
			{
				// Gas block for upgrades
				gas = 0;
			}
		}
	}
	if (supply == 200)
	{
		for (const auto & unit : Armories)
		{
			if (unit->isCompleted() && unit->isIdle())
			{
				std::vector<sc2::AvailableAbility> abilities = m_bot.Query()->GetAbilitiesForUnit(unit->getUnit_ptr(), true).abilities;
				// Weapons and armor research has consecutive numbers
				// First weapons
				for (sc2::AvailableAbility & ability : abilities)
				{
					if (ability.ability_id == sc2::ABILITY_ID::RESEARCH_TERRANVEHICLEWEAPONS)
					{
						const int weaponMech = m_bot.getWeaponMech();
						if ((weaponMech == 0 && gas >= 100) || (weaponMech == 1 && gas >= 175) || (weaponMech == 2 && gas >= 250))
						{
							if ((weaponMech == 0 && minerals >= 100) || (weaponMech == 1 && minerals >= 175) || (weaponMech == 2 && minerals >= 250))
							{
								Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_TERRANVEHICLEWEAPONS, m_bot);
							}
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Weapon ++" << std::endl;
							return;
						}
					}
				}
			}
		}
	}
	// Factories
	for (const auto & unit : Factories)
	{
		// Any finished Factory
		if (unit->isCompleted() && !unit->isFlying())
		{
			// that is idle
			if (unit->isIdle())
			{
				// if it is only a factory build addon
				if (!unit->getAddOnTag())
				{
					// Build a reactor for the first starport
                    if (numStarport == 0 || (numStarport == 1 && numStarportFinished == 0))
					{
						if (minerals >= 50 && gas >= 50 && numStarport == 1)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_REACTOR_FACTORY, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "ReactorFactory" << std::endl;
							return;
						}
					}
					else if (Starports.size() > 0 && (Starports.front()->getAddOnTag() || (Starports.front()->getOrders().size() > 0 && Starports.front()->getOrders().front().ability_id == sc2::ABILITY_ID::BUILD_REACTOR)))
					{
						if (minerals >= 50 && gas >= 25)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "TechlabFactory" << std::endl;
							return;
						}
					}
				}
				else if (m_bot.UnitInfo().getUnit(unit->getAddOnTag())->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)
				{
					const size_t numWidowMines = m_bot.UnitInfo().getUnitTypeCount(Players::Self, std::vector<sc2::UNIT_TYPEID>{ sc2::UNIT_TYPEID::TERRAN_WIDOWMINE, sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED });
					const size_t numTanks = m_bot.UnitInfo().getUnitTypeCount(Players::Self, std::vector<sc2::UNIT_TYPEID>{ sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED });
					const size_t numThors = m_bot.UnitInfo().getUnitTypeCount(Players::Self, std::vector<sc2::UNIT_TYPEID>{ sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP });
					if (numArmories > 0 && m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Zerg && m_bot.UnitInfo().getUnitTypeCount(Players::Enemy, sc2::UNIT_TYPEID::ZERG_MUTALISK) > 0 && numThors == 0)
					{
						if (minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_THOR).mineralCost && gas >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_THOR).gasCost && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_THOR).supplyCost)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_THOR, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Thor" << std::endl;
							return;
						}
					}
					else if (numTanks > 0 && numWidowMines == 0 && minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).mineralCost && gas >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).gasCost && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).supplyCost)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_WIDOWMINE, m_bot);
                        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "WM" << std::endl;
						return;
					}
					else if (minerals >= 150 && gas >= 125 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_SIEGETANK).supplyCost)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_SIEGETANK, m_bot);
                        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Tank" << std::endl;
						return;
					}
				}
			}
		}
	}
	// Starports
	for (const auto & unit : Starports)
	{
		// Any finished Starport
		if (unit->isCompleted())
		{
			// that is idle
			const auto addon = m_bot.UnitInfo().getUnit(unit->getAddOnTag());
            if (unit->isIdle() || (unit->getOrders().size() == 1 && addon && addon->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR))
			{
				// if it is only a Starport build addon
				if (!addon)
				{
					// The reactor for the first starport should be with the factory
					for (const auto & factory : Factories)
					{
						if ((factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->getOrders().empty() && factory->getAddOnTag() && !unit->isFlying()) || (factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING && factory->getOrders().empty() && unit->isFlying()))
						{
							Util::swapBuildings(unit, factory, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Swap" << std::endl;
							return;
						}
						else
						{
							// Something went wrong. The Factory has a techlab. Build the reactor yourself.
							if (factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->getOrders().empty() && (!factory->getAddOnTag() || (m_bot.UnitInfo().getUnit(factory->getAddOnTag())->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)))
							{
								if (minerals >= 50 && gas >= 25)
								{
									Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_REACTOR_STARPORT, m_bot);
                                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "ReactorStarport" << std::endl;
									return;
								}
							}
						}
					}
				}
				else
				{
					if (m_liberatorRequested)
					{
						if (gas >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_LIBERATOR).gasCost && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_LIBERATOR).supplyCost)
						{
							if (minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_LIBERATOR).mineralCost)
							{
								m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_LIBERATOR);
								m_liberatorRequested = false;
                                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Liberator" << std::endl;
							}
							else
							{
                                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving for Liberator" << std::endl;
							}
							return;
						}
					}
					else
					{
						const size_t numMedivacs = m_bot.UnitInfo().getUnitTypeCount(sc2::Unit::Alliance::Self, sc2::UNIT_TYPEID::TERRAN_MEDIVAC);
						const size_t numVikings = m_bot.UnitInfo().getUnitTypeCount(sc2::Unit::Alliance::Self, std::vector<sc2::UNIT_TYPEID>{ sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT });
						if ((!m_vikingRequested && numMedivacs < 9) || (m_vikingRequested && numMedivacs <= numVikings))
						{
							if (gas >= 100 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MEDIVAC).supplyCost)
							{
								if (minerals >= 100)
								{
									m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_MEDIVAC);
                                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Medivac" << std::endl;
								}
								else
								{
                                    Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Saving for Medivac" << std::endl;
								}
								return;
							}
						}
						else if (numVikings <= numMedivacs)
						{
							if (minerals >= 150 && gas >= 75 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER).supplyCost)
							{
								m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_VIKINGFIGHTER);
                                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Viking" << std::endl;
								return;
							}
						}
					}
				}
			}
		}
    }
    //BARRACKS
    //reaper
    if (m_scoutRequested)
    {
        const ProductionStatus reaper = pleaseTrain(sc2::UNIT_TYPEID::TERRAN_MARAUDER, minerals, gas);
        if (reaper == ProductionStatus::success)
        {
            m_scoutRequested = false;
            return;
        }
        else if (reaper == ProductionStatus::notEnoughMinerals)
        {
            return;
        }
    }
    // AddOns
    if (numBReactor + numBTechLab < numRaxFinished)
    {
        sc2::UNIT_TYPEID addOnType = sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB;
        if (numBReactor == 0 || numBTechLab >= getSuggestedNumTechLabs())
        {
            addOnType = sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR;
        }
        if (pleaseTrain(addOnType, minerals, gas) == ProductionStatus::success)
        {
            return;
        }
    }
    // Units
    else
    {
        if (m_bot.UnitInfo().getUnits(Players::Enemy, Util::getAntiMedivacTypes()).size() > 3)
        {
            if (pleaseTrain(sc2::UNIT_TYPEID::TERRAN_MARAUDER, minerals, gas) == ProductionStatus::success)
            {
                return;
            }
        }
        if (pleaseTrain(sc2::UNIT_TYPEID::TERRAN_MARINE, minerals, gas) == ProductionStatus::success)
        {
            return;
        }
    }



	if (minerals >= 100 && numBases >= 2)  // m_bot.GetPlayerRace(Players::Enemy) != sc2::Race::Terran &&
	{
		const CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER);
		if (Bunker.size() + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BUNKER) < 1)
		{
			m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BUNKER), BUILDING, false));
            Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Bunker" << std::endl;
			return;
		}
	}

	// Engineering bay
	// We start with one. We want to built it after the starport has finished
	if (minerals >= 125
		&& (((numStarport >= 1 || m_turretsRequested) && numEngibays + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY) == 0)
		|| (numEngibays + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY) == 1 && numEngibaysFinished == 1)))
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY), BUILDING, false));
        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Engineering Bay" << std::endl;
		return;
	}

	// Factories. For now just one.
	if (numBases> 1 && minerals >= 150 && gas >= 100 && numFactory + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_FACTORY) == 0 && bReactor.size() > 0)
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_FACTORY), BUILDING, false));
        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Factory" << std::endl;
		return;
	}
	// Starports. For now just one.
	if (numBases> 1 && minerals >= 150 && gas >= 100 && numFactoryFinished > 0 && numStarport + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_STARPORT) == 0)
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_STARPORT), BUILDING, false));
        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Starport" << std::endl;
		return;
	}
	// On one base save a little to get an expansion
	if (numBases == 1)
	{
		if (true)  // m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
		{
			if (numDepotsFinished > 0 && minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 2)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Barracks" << std::endl;
				return;
			}
		}
		else
		{
			if (numDepotsFinished > 0 && minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Barracks" << std::endl;
				return;
			}
		}
	}
	// On two bases let the starport start first
	else
	{
		if (numRax == 1 || numStarport > 0)
		{
			if (minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 3 * numBases - 4 && numRax <= 9)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
                Drawing::cout{m_bot.Observation()->GetGameLoop()} << "Barracks" << std::endl;
				return;
			}
		}
	}
	// expanding
	if (minerals >= 400 && numBases - numBasesFinished + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) == 0 && m_bot.Bases().getNextExpansion(Players::Self) != sc2::Point2D{})
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER), BUILDING, false));
        Drawing::cout{m_bot.Observation()->GetGameLoop()} << "CC" << std::endl;
		return;
	}
	// When we did nothing...
	m_defaultMacroSleep = m_defaultMacroSleepMax;
	return;
}

ProductionStatus ProductionManager::pleaseTrain(const sc2::UNIT_TYPEID & unitType, const size_t minerals, const size_t gas)
{
    const auto & data = m_bot.Data(unitType);

    if (gas >= data.gasCost)
    {
        if (minerals >= data.mineralCost)
        {
            if(static_cast<size_t>(m_bot.Observation()->GetFoodUsed()) <= 200 - data.supplyCost)
            {
                for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self, data.whatBuilds))
                {
                    bool isItIdle = false;
                    if (unit->isCompleted())
                    {
                        const auto addon = unit->getAddOn();
                        if (addon)
                        {
                            if (data.needsTechlab)
                            {
                                if (unit->isIdle() && addon->isType(sc2::UNIT_TYPEID::TERRAN_TECHLAB))
                                {
                                    isItIdle = true;
                                }
                            }
                            else
                            {
                                if (unit->isIdle() || (addon->isType(sc2::UNIT_TYPEID::TERRAN_REACTOR) && unit->getOrders().size() < 2))
                                {
                                    isItIdle = true;
                                }
                            }
                        }
                        else
                        {
                            if (unit->isIdle() && (m_bot.Data(unitType).isAddon || unitType == sc2::UNIT_TYPEID::TERRAN_REAPER || m_bot.underAttack()))
                            {
                                isItIdle = true;
                            }
                        }
                        if (isItIdle)
                        {
                            Micro::SmartAbility(unit, data.buildAbility, m_bot);
                            Drawing::cout{m_bot.Observation()->GetGameLoop()} << Util::GetUnitText(unit->getUnitType()) << " --> " << Util::GetAbilityText(data.buildAbility) << "-->" << Util::GetUnitText(unitType) << std::endl;
                            return ProductionStatus::success;
                        }
                    }
                }
                return ProductionStatus::noIdleProduction;
            }
            return ProductionStatus::notEnoughSupply;
        }
        return ProductionStatus::notEnoughMinerals;
    }
    return ProductionStatus::notEnoughGas;
}
void ProductionManager::requestScout()
{
	m_scoutRequested = true;
}

void ProductionManager::requestVikings()
{
	m_vikingRequested = true;
}

void ProductionManager::requestTurrets()
{
	m_turretsRequested = true;
}

void ProductionManager::requestLiberator()
{
	m_liberatorRequested = true;
}

void ProductionManager::requestScan()
{
	++m_scansRequested;
}

void ProductionManager::usedScan(const size_t i)
{
	// if we needed one...
	if (m_scansRequested > 1+i)
	{
		m_scansRequested -= i;
	}
	else
	{
		m_scansRequested = 1;
	}
}

size_t ProductionManager::buildingsFinished(const CUnits units) const
{
    size_t numBuildingsFinished = 0;
	for (const auto & unit : units)
	{
		if (unit->isCompleted())
		{
			numBuildingsFinished++;
		}
	}
	return numBuildingsFinished;
}


void ProductionManager::needCC()
{
	const CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes());
	const int numBases = static_cast<int>(CommandCenters.size());
	const int numBasesFinished = buildingsFinished(CommandCenters);
	if (numBases - numBasesFinished + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) == 0 && m_bot.Bases().getOccupiedBaseLocations(Players::Self).size() + m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).size() < m_bot.Bases().getBaseLocations().size())
	{
		m_needCC = true;
	}
}

size_t ProductionManager::howOftenQueued(sc2::UnitTypeID type)
{
	int numQueued = 0;
	for (const auto & b : m_newQueue)
	{
		if (b.m_type.getUnitTypeID() == type)
		{
			numQueued++;
		}
	}
	return numQueued + m_buildingManager.getNumBuildingsQueued(type);
}

bool ProductionManager::tryingToExpand() const
{
	return m_buildingManager.tryingToExpand();
}

void ProductionManager::drawProductionInformation()
{
	if (!m_bot.Config().DrawProductionInfo)
	{
		return;
	}

	std::stringstream ss;
	ss << "Production Information\n";

	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (unit->isCompleted())
		{
			// ss << sc2::UnitTypeToName(unit.unit_type) << " " << unit.build_progress << "\n";
		}
	}
	for (const auto & b : m_newQueue)
	{
		ss << "\n" << sc2::UnitTypeToName(b.m_type.getUnitTypeID());
	}
	Drawing::drawTextScreen(m_bot, sc2::Point2D(0.01f, 0.01f), ss.str(), sc2::Colors::Yellow);
}


size_t ProductionManager::getSuggestedNumTechLabs() const
{
    // Not very clever yet.
    return static_cast<size_t>(std::max(1, static_cast<int>(m_bot.UnitInfo().getUnitTypeCount(Players::Enemy, sc2::UNIT_TYPEID::TERRAN_FACTORY, false))));
}
