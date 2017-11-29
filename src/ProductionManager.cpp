#include "ProductionManager.h"
#include "Util.h"
#include "CCBot.h"
#include "Micro.h"



const int UNIT = 5;
const int BUILDING = 3;

//it seems to many commands confuse the engine
int defaultMacroSleep = 0;
const int defaultMacroSleepMax = 10;
bool canBuildAddon = true;
int addonCounter = 0;
const int maxAddonCounter = 5;

ProductionManager::ProductionManager(CCBot & bot)
    : m_bot             (bot)
    , m_buildingManager (bot)
    , m_queue           (bot)
{

}

void ProductionManager::setBuildOrder(const BuildOrder & buildOrder)
{
    m_queue.clearAll();

    for (size_t i(0); i<buildOrder.size(); ++i)
    {
        m_queue.queueAsLowestPriority(buildOrder[i], true);
    }
}


void ProductionManager::onStart()
{
    m_buildingManager.onStart();
    setBuildOrder(m_bot.Strategy().getOpeningBookBuildOrder());
	m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT), BUILDING, false));
}

void ProductionManager::onFrame()
{
	//Proxy Check

	defaultMacro();
	
	detectBuildOrderDeadlock();
	//ADD THINGS TO THE QUEUE BEFORE THIS! IT WILL BE TAKEN FROM THE QUEUE BUT ONLY APPLIED AT THE END OF THE FRAME!!


	
	// check the _queue for stuff we can build
    manageBuildOrderQueue();

    // TODO: if nothing is currently building, get a new goal from the strategy manager
    // TODO: detect if there's a build order deadlock once per second
    // TODO: triggers for game things like cloaked units etc


	

    m_buildingManager.onFrame();
    drawProductionInformation();
}

// on unit destroy
void ProductionManager::onUnitDestroy(const sc2::Unit * unit)
{
    // TODO: might have to re-do build order if a vital unit died
}

void ProductionManager::manageBuildOrderQueue()
{
    // if there is nothing in the queue, oh well
    if (m_queue.isEmpty())
    {
        return;
    }

    // the current item to be used
    BuildOrderItem & currentItem = m_queue.getHighestPriorityItem();

    // while there is still something left in the queue
    while (!m_queue.isEmpty())
    {
        // this is the unit which can produce the currentItem
        const sc2::Unit * producer = getProducer(currentItem.type);

        // check to see if we can make it right now
        bool canMake = canMakeNow(producer, currentItem.type);

        // TODO: if it's a building and we can't make it yet, predict the worker movement to the location

        // if we can make the current item
        if (producer && canMake)
        {
            // create it and remove it from the _queue
            create(producer, currentItem);
            m_queue.removeCurrentHighestPriorityItem();

            // don't actually loop around in here
            break;
        }
        // otherwise, if we can skip the current item
        else if (m_queue.canSkipItem())
        {
            // skip it
            m_queue.skipItem();

            // and get the next one
            currentItem = m_queue.getNextHighestPriorityItem();
        }
        else
        {
            // so break out
            break;
        }
    }
}

const sc2::Unit * ProductionManager::getProducer(const BuildType & type, sc2::Point2D closestTo)
{
    // get all the types of units that can build this type
    auto & producerTypes = m_bot.Data(type).whatBuilds;

    // make a set of all candidate producers
    std::vector<const sc2::Unit *> candidateProducers;
    for (auto unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        // reasons a unit can not train the desired type
        if (std::find(producerTypes.begin(), producerTypes.end(), unit->unit_type) == producerTypes.end()) { continue; }
        if (unit->build_progress < 1.0f) { continue; }
        if (m_bot.Data(unit->unit_type).isBuilding && unit->orders.size() > 0) { continue; }
        if (unit->is_flying) { continue; }

        // TODO: if unit is not powered continue
        // TODO: if the type is an addon, some special cases
        // TODO: if the type requires an addon and the producer doesn't have one

        // if we haven't cut it, add it to the set of candidates
        candidateProducers.push_back(unit);
    }

    return getClosestUnitToPosition(candidateProducers, closestTo);
}

const sc2::Unit * ProductionManager::getClosestUnitToPosition(const std::vector<const sc2::Unit *> & units, sc2::Point2D closestTo)
{
    if (units.size() == 0)
    {
        return 0;
    }

    // if we don't care where the unit is return the first one we have
    if (closestTo.x == 0 && closestTo.y == 0)
    {
        return units[0];
    }

    const sc2::Unit * closestUnit = nullptr;
    double minDist = std::numeric_limits<double>::max();

    for (auto & unit : units)
    {
        double distance = Util::Dist(unit->pos, closestTo);
        if (!closestUnit || distance < minDist)
        {
            closestUnit = unit;
            minDist = distance;
        }
    }

    return closestUnit;
}

// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::create(const sc2::Unit * producer, BuildOrderItem & item)
{
    if (!producer)
    {
        return;
    }

    // if we're dealing with a building
    // TODO: deal with morphed buildings & addons
    if (m_bot.Data(item.type).isBuilding)
    {
        // send the building task to the building manager
		if (Util::IsTownHallType(item.type.getUnitTypeID()))
		{
			m_buildingManager.addBuildingTask(item.type.getUnitTypeID(), m_bot.Bases().getNextExpansion(Players::Self));
		}
		//We want to switch addons from factory to starport. So better build them close.
		else if (item.type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORT)
		{
			const sc2::Units Factories = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_FACTORY , sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
			if (Factories.size()>0)
			{
				m_buildingManager.addBuildingTask(item.type.getUnitTypeID(), Factories[0]->pos);
			}
			else
			{
				
				m_buildingManager.addBuildingTask(item.type.getUnitTypeID(),m_bot.Bases().getBuildingLocation());
			}
		}
		else if (item.type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_BUNKER)
		{
			//Just because of pocket bases-.-
			sc2::Point2D bPoint(m_bot.Bases().getRallyPoint());
			const BaseLocation * homeBase = m_bot.Bases().getPlayerStartingBaseLocation(Players::Self);
			//Any enemy base is fine
			const BaseLocation * enemyBase = *m_bot.Bases().getOccupiedBaseLocations(Players::Enemy).begin();
			if (homeBase && enemyBase)
			{
				sc2::Point2D homeBasePos = homeBase->getBasePosition();
				if (enemyBase->getGroundDistance(bPoint) > enemyBase->getGroundDistance(homeBasePos))
				{
					bPoint = homeBasePos;
				}
			}
			m_buildingManager.addBuildingTask(item.type.getUnitTypeID(), bPoint);
		}
		else
		{
			m_buildingManager.addBuildingTask(item.type.getUnitTypeID(), m_bot.Bases().getBuildingLocation());
		}
    }
    // if we're dealing with a non-building unit
    else if (item.type.isUnit())
    {
        Micro::SmartTrain(producer, item.type.getUnitTypeID(), m_bot);
    }
    else if (item.type.isUpgrade())
    {
        Micro::SmartAbility(producer, m_bot.Data(item.type.getUpgradeID()).buildAbility, m_bot);
    }
}

bool ProductionManager::canMakeNow(const sc2::Unit * producer, const BuildType & type)
{
    if (!producer || !meetsReservedResources(type))
    {
        return false;
    }

    sc2::AvailableAbilities available_abilities = m_bot.Query()->GetAbilitiesForUnit(producer);

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

bool ProductionManager::detectBuildOrderDeadlock()
{
    // TODO: detect build order deadlocks here

	//I really dont get it
	if (m_queue.size() == 2 && m_queue[0].type.getUnitTypeID() == sc2::UNIT_TYPEID::TERRAN_REFINERY&&m_queue[1].type.getUnitTypeID() == sc2::UNIT_TYPEID::TERRAN_REFINERY)
	{
		m_queue.removeHighestPriorityItem();
	}
		return false;
}

int ProductionManager::getFreeMinerals()
{
	if (m_buildingManager.getReservedMinerals() > 500)
	{
		m_buildingManager.resetFreeMinerals();
	}
	return m_bot.Observation()->GetMinerals() - m_buildingManager.getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
    return m_bot.Observation()->GetVespene() - m_buildingManager.getReservedGas();
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::meetsReservedResources(const BuildType & type)
{
    // return whether or not we meet the resources
    return (m_bot.Data(type).mineralCost <= getFreeMinerals()) && (m_bot.Data(type).gasCost <= getFreeGas());
}

void ProductionManager::drawProductionInformation()
{
    if (!m_bot.Config().DrawProductionInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Production Information\n\n";

    for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (unit->build_progress < 1.0f)
        {
            //ss << sc2::UnitTypeToName(unit.unit_type) << " " << unit.build_progress << "\n";
        }
    }

    ss << m_queue.getQueueInformation();

    m_bot.Map().drawTextScreen(sc2::Point2D(0.01f, 0.01f), ss.str(), sc2::Colors::Yellow);
}

void ProductionManager::defaultMacro()
{

	//Maybe too fast addon command confuse the engine?!
	++defaultMacroSleep;
	if (defaultMacroSleep < defaultMacroSleepMax)
	{
		return;
	}
	defaultMacroSleep = 0;

	//It seems that for now player 2 can not build addons. Since we do not know which one we are, we first try a few times
	if (canBuildAddon && addonCounter > maxAddonCounter)
	{
		m_bot.Actions()->SendChat("Seems I am player 2. Unable to build addons.");
		canBuildAddon = false;
	}

	int32_t minerals = getFreeMinerals();
	//without money...
	if (minerals < 50)
	{
		defaultMacroSleep = defaultMacroSleepMax;
		return;
	}
	int32_t gas = getFreeGas();//m_bot.Observation()->GetVespene();
	//for every production building we need 2 supply free
	int32_t supply = m_bot.Observation()->GetFoodUsed();
	int32_t supplyCap=m_bot.Observation()->GetFoodCap();
	
	const sc2::Units Depots = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT , sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED }));
	const int numDepots = Depots.size();
	const int numDepotsFinished = buildingsFinished(Depots);

	const sc2::Units CommandCenters = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER , sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND , sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS }));
	const int numBases = CommandCenters.size();
	const int numBasesFinished = buildingsFinished(CommandCenters);

	const sc2::Units Rax = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit( sc2::UNIT_TYPEID::TERRAN_BARRACKS));
	const int numRax = Rax.size();
	const int numRaxFinished = buildingsFinished(Rax);

	const sc2::Units Starports = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_STARPORT , sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING }));
	const int numStarport = Starports.size();
	const int numStarportFinished = buildingsFinished(Starports);

	const sc2::Units Factories = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_FACTORY , sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
	const int numFactory = Factories.size();
	const int numFactoryFinished = buildingsFinished(Factories);

	//const sc2::Units mineralNodes = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Neutral, sc2::IsUnits({ sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD , sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD750 , sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD , sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD750}));
	

	if (supplyCap < 200 && supplyCap - supply < (numBasesFinished + 2 * numRaxFinished))
	{
		//count the building and requested 
		const int numBuildingDepots = numDepots-numDepotsFinished;
		
		if (numBuildingDepots + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < numBasesFinished)
		{
			m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT), BUILDING, false));
			std::cout << "Depot" << std::endl;
			return;
		}
	}
	//Every Base has to build a worker until 66 workers.
	if (m_bot.Observation()->GetFoodWorkers() < 66)
	{
		for (auto & unit : CommandCenters)
		{
			if (unit->build_progress == 1.0f)
			{
				//Sometimes we have a problem here
				if (unit->assigned_harvesters == 0 || unit->energy > 60)
				{
					m_bot.OnBuildingConstructionComplete(unit);
				}
				//Before building a worker, make sure it is a OC
				if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER && (unit->orders.empty() || unit->orders[0].ability_id.ToType() != sc2::ABILITY_ID::MORPH_ORBITALCOMMAND) && numRaxFinished > 0)
				{
					if (minerals > 150)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
					}
					std::cout << "OC" << std::endl;
					return;
				}
				if (unit->energy >= 50)
				{
					const sc2::Unit * mineralPatch = Util::getClostestMineral(unit->pos, m_bot);
					if (mineralPatch && mineralPatch->Visible == sc2::Unit::DisplayType::Visible)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, mineralPatch);
					}
				}
				int numOfRefinaries = m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_REFINERY, false) + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_REFINERY);
				if (numRax > 0 && minerals >= 75 && ((m_bot.Observation()->GetFoodWorkers() >= 15 && numOfRefinaries == 0) || (m_bot.Observation()->GetFoodWorkers() >= 21 && numOfRefinaries == 1)))
				{
					m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_REFINERY), BUILDING, false));
					std::cout << "Refinary" << std::endl;
					return;
				}
				if (unit->orders.empty() && unit->assigned_harvesters < unit->ideal_harvesters + 3)
				{
					//m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SCV), UNIT, false));
					m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
					std::cout << "TrainSCV" << std::endl;
					return;
				}

			}
		}
	}
	//Upgrades
	const sc2::Units bTechLab = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
	for (auto & unit : bTechLab)
	{
		if (unit->build_progress == 1 && unit->orders.empty())
		{
			auto upgrades=m_bot.Observation()->GetUpgrades();
			if (upgrades.size() == 0)
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_COMBATSHIELD);
					}
					std::cout << "CombatShield" << std::endl;
					return;
				}
			}
			else if (upgrades.size() == 1)
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_STIMPACK);
					}
					std::cout << "Stim" << std::endl;
					return;
				}
			}
			else if (upgrades.size() == 2)
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 50)
				{
					if (minerals >= 50)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS);
					}
					std::cout << "Concussiv" << std::endl;
					return;
				}
			}
			
		}
	}

	//Every rax has to build
	const sc2::Units bReactor = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR));
	for (auto & unit : Rax)
	{
		//Any finished rax
		if (unit->build_progress == 1)
		{
			//that is idle
			if (unit->orders.empty() || (unit->orders.size() == 1 && unit->add_on_tag && m_bot.Observation()->GetUnit(unit->add_on_tag)->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR))
			{
				//Building the scout has priority
				if (m_scoutRequested)
				{
					if (gas >= 50)
					{
						if (minerals >= 50)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_REAPER);
							m_scoutRequested = false;
						}
						std::cout << "Scout" << std::endl;
						return;
					}
				}
				//if it is only a rax build addon
				if (canBuildAddon && !unit->add_on_tag)
				{
					//The second barracks gets a lab
					if (bTechLab.size() == 1 || bReactor.size() == 0)
					{
						if (minerals >= 50 && gas >= 50)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::BUILD_REACTOR_BARRACKS);
							if (bReactor.size() == 0)
							{
								++addonCounter;
							}
							std::cout << "ReactorBarracks" << std::endl;
							return;
						}
					}
					else
					{
						if (minerals >= 50 && gas >= 25)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS);
							std::cout << "TechlabBarracks" << std::endl;
							return;
						}
					}
				}
				else if (minerals >= 50 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).supplyCost)
				{
					if (unit->add_on_tag && minerals >= 100 && m_bot.Observation()->GetUnit(unit->add_on_tag)->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR && supply <= 200 - 2*m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MEDIVAC).supplyCost)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
						std::cout << "Marine" << std::endl;
					}
					m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
					std::cout << "Marine" << std::endl;
					return;
				}
			}
		}
	}
	for (auto & unit : Factories)
	{
		//Any finished Factory
		if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORY && unit->build_progress == 1)
		{
			//that is idle
			if (canBuildAddon && unit->orders.empty())
			{
				//if it is only a factory build addon
				if (!unit->add_on_tag)
				{
					//Build a reactor for the first starport
					if (numStarport==0 || numStarport==1 && numStarportFinished==0)
					{
						if (minerals >= 50 && gas >= 50 && numStarport == 1)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::BUILD_REACTOR_FACTORY);
							std::cout << "ReactorFactory" << std::endl;
							return;
						}
					}
					else
					{
						if (minerals >= 50 && gas >= 25)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY);
							std::cout << "TechlabFactory" << std::endl;
							return;
						}
					}
				}
				else if (m_bot.Observation()->GetUnit(unit->add_on_tag)->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)
				{
					if (minerals >= 150 && gas >= 125 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_SIEGETANK).supplyCost)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SIEGETANK);
						std::cout << "Tank" << std::endl;
						return;
					}
				}
			}
		}
	}
	for (auto & unit : Starports)
	{
		//Any finished Starport
		if (unit->build_progress == 1)
		{
			//that is idle
			if (unit->orders.empty() || unit->add_on_tag && m_bot.Observation()->GetUnit(unit->add_on_tag)->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR)
			{
				//if it is only a Starport build addon
				if (canBuildAddon && !unit->add_on_tag)
				{
					//The reactor for the first starport should be with the factory
					for (auto & factory : Factories)
					{
						if ((factory->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->orders.empty() && factory->add_on_tag && !unit->is_flying)||(factory->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING && factory->orders.empty()&& unit->is_flying))
						{
							Util::swapBuildings(unit, factory,m_bot);
							std::cout << "Swap" << std::endl;
							return;
						}
						else
						{
							//Something went wrong. The Factory has a techlab. Build the reactor yourself.
							if (factory->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->orders.empty() && factory->add_on_tag && m_bot.Observation()->GetUnit(factory->add_on_tag)->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)
							{
								if (minerals >= 50 && gas >= 25)
								{
									m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::BUILD_REACTOR_STARPORT);
									std::cout << "ReactorStarport" << std::endl;
									return;
								}
							}
						}
					}
				}
				else
				{
					if (minerals >= 100 && gas >= 100 && supply<=200-m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MEDIVAC).supplyCost)
					{
						m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MEDIVAC);
						if (minerals >= 200 && gas >= 200 && unit->orders.size()==1 && supply <= 200 - 2*m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MEDIVAC).supplyCost)
						{
							m_bot.Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MEDIVAC);
						}
						std::cout << "Medivac" << std::endl;
						return;
					}
				}
			}
		}
	}
	if (minerals >= 100 && m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Zerg && numBases == 2)
	{
		const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BUNKER }));
		if (Bunker.size() + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BUNKER) < 1)
		{
			m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BUNKER), BUILDING, false));
		}
	}

	//Factories. For now just one.
	if (minerals >= 150 && gas >= 100 && numFactory + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_FACTORY) == 0 && (bReactor.size()>0 || !canBuildAddon))
	{
		m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_FACTORY), BUILDING, false));
		std::cout << "Factory" << std::endl;
		return;
	}
	//Starports. For now just one.
	if (minerals >= 150 && gas >= 100 && numFactoryFinished>0 && numStarport + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_STARPORT) ==0)
	{
		m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_STARPORT), BUILDING, false));
		std::cout << "Starport" << std::endl;
		return;
	}
	//On one base save a little to get an expansion
	if (numBases == 1)
	{
		if (Depots.size() > 0 && minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1)
		{
			m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
			std::cout << "Barracks" << std::endl;
			return;
		}
	}
	//On two bases let the starport start first
	else
	{
		if (numRax==1 || numStarport>0)
		{
			if (minerals >= 150 && numRax + 2 + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 3 * numBases)
			{
				m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
				std::cout << "Barracks" << std::endl;
				return;
			}
		}
	}
	//expanding
	if (minerals >= 400 && numBases-numBasesFinished+howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)==0)
	{
		m_queue.queueItem(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER), BUILDING, false));
		std::cout << "CC" << std::endl;
		return;
	}
	//When we did nothing...
	defaultMacroSleep = defaultMacroSleepMax;
	return;
}

void ProductionManager::requestScout()
{
	m_scoutRequested = true;
}

int ProductionManager::buildingsFinished(sc2::Units units)
{
	int numBuildingsFinished = 0;
	for (auto & unit : units)
	{
		if (unit->build_progress==1.0f)
		{
			numBuildingsFinished++;
		}
	}
	return numBuildingsFinished;
}

int ProductionManager::howOftenQueued(sc2::UnitTypeID type)
{
	return m_queue.howOftenQueued(type) + m_buildingManager.getNumBuildingsQueued(type);
}