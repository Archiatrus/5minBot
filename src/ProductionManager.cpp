#include "ProductionManager.h"
#include "Util.h"
#include "CCBot.h"
#include "Micro.h"
#include "sc2api/sc2_score.h"


const int UNIT = 5;
const int BUILDING = 3;

//it seems to many commands confuse the engine
int defaultMacroSleep = 0;
const int defaultMacroSleepMax = 5;

bool startedResearch = false;

ProductionManager::ProductionManager(CCBot & bot)
	: m_bot(bot)
	, m_buildingManager(bot)
	, m_scoutRequested(false)
	, m_vikingRequested(false)
	, m_scansRequested(0)
{

}

void ProductionManager::onStart()
{
	m_buildingManager.onStart();
}

void ProductionManager::onFrame()
{

	defaultMacro();
	
	detectBuildOrderDeadlock();
	//ADD THINGS TO THE QUEUE BEFORE THIS POINT! IT WILL BE TAKEN FROM THE QUEUE BUT ONLY APPLIED AT THE END OF THE FRAME!!


	
	// check the _queue for stuff we can build
	manageBuildOrderQueue();

	// TODO: if nothing is currently building, get a new goal from the strategy manager
	// TODO: detect if there's a build order deadlock once per second
	// TODO: triggers for game things like cloaked units etc


	

	m_buildingManager.onFrame();
	drawProductionInformation();
}

// on unit destroy
void ProductionManager::onUnitDestroy(const CUnit_ptr unit)
{
	// TODO: might have to re-do build order if a vital unit died
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
	//We want to switch addons from factory to starport. So better build them close.
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
	else if (item.m_type.getUnitTypeID().ToType() == sc2::UNIT_TYPEID::TERRAN_BUNKER)
	{
		sc2::Point2D bPoint;
		if (m_bot.Map().hasPocketBase())
		{
			const sc2::Point2D startPoint(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase());
			const float startHeight = m_bot.Observation()->TerrainHeight(startPoint);
			sc2::Point2D currentPos(startPoint);
			const sc2::Point2D enemyPoint = m_bot.Observation()->GetGameInfo().enemy_start_locations.front();
			BaseLocation * const enemyBaseLocation = m_bot.Bases().getBaseLocation(enemyPoint);
			const float stepSize = 2.0;
			const sc2::Point2D xMove(stepSize, 0.0f);
			const sc2::Point2D yMove(0.0f, stepSize);
			int currentWalkingDistance = enemyBaseLocation->getGroundDistance(startPoint);
			bool foundNewPos = true;
			while (foundNewPos)
			{
				foundNewPos = false;
				for (float i = -1.0f; i <= 1.0f; ++i)
				{
					for (float j = -1.0f; j <= 1.0f; ++j)
					{
						if (i != 0.0f || j != 0.0f)
						{
							const sc2::Point2D newPos = currentPos + i*xMove + j*yMove;
							const int dist = enemyBaseLocation->getGroundDistance(newPos);
							if (m_bot.Observation()->TerrainHeight(newPos) == startHeight && dist > 0 && currentWalkingDistance > dist)
							{
								currentWalkingDistance = dist;
								currentPos = newPos;
								foundNewPos = true;
								break;
							}
						}
					}
					if (foundNewPos)
					{
						break;
					}
				}
			}
			bPoint = currentPos;
		}
		else
		{
			bPoint = m_bot.Bases().getRallyPoint();
		}
		m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), bPoint);
	}
	else
	{
		m_buildingManager.addBuildingTask(item.m_type.getUnitTypeID(), m_bot.Bases().getBuildingLocation());
	}
}

bool ProductionManager::detectBuildOrderDeadlock()
{
	// TODO: detect build order deadlocks here
	return false;
}

int ProductionManager::getFreeMinerals()
{
	int reserved = m_buildingManager.getReservedMinerals();
	if (reserved > 1000 || reserved < 0)
	{
		m_buildingManager.resetFreeMinerals();
		reserved = 0;
	}
	return m_bot.Observation()->GetMinerals() - m_buildingManager.getReservedMinerals();
}

int ProductionManager::getFreeGas()
{
	return m_bot.Observation()->GetVespene() - m_buildingManager.getReservedGas();
}

void ProductionManager::defaultMacro()
{
	//This avoids repeating the same command twice.
	++defaultMacroSleep;
	if (defaultMacroSleep < defaultMacroSleepMax)
	{
		return;
	}
	defaultMacroSleep = 0;

	//Even without money we can drop mules
	const CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes());
	int scansAvailable = 0;
	for (const auto & unit : CommandCenters)
	{
		if (unit->isCompleted())
		{
			if (unit->getEnergy() >= 50)
			{
				scansAvailable+=static_cast<int>(std::floor(unit->getEnergy()/50.0f));
				if (scansAvailable > m_scansRequested)
				{
					const CUnit_ptr mineralPatch = Util::getClostestMineral(unit->getPos(), m_bot);
					if (mineralPatch && mineralPatch->getDisplayType() == sc2::Unit::DisplayType::Visible)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, mineralPatch,m_bot);
					}
				}
			}
			//Sometimes we have a problem here
			if (unit->getAssignedHarvesters() == 0 || unit->getEnergy() > 60)
			{
				m_bot.Bases().assignTownhallToBase(unit);
			}
		}
	}


	int32_t minerals = getFreeMinerals();
	//but not much else
	if (minerals < 50)
	{
		defaultMacroSleep = defaultMacroSleepMax;
		return;
	}
	int32_t gas = getFreeGas();
	//for every production building we need 2 supply free
	int32_t supply = m_bot.Observation()->GetFoodUsed();
	int32_t supplyCap = m_bot.Observation()->GetFoodCap();
	

	//Maybe it would be smarter to search for all buildings first and then search through the resulting vector
	const CUnits Depots = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT , sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED }));
	const int numDepots = static_cast<int>(Depots.size());
	const int numDepotsFinished = buildingsFinished(Depots);

	const int numBases = static_cast<int>(CommandCenters.size());
	const int numBasesFinished = buildingsFinished(CommandCenters);

	const CUnits Rax = m_bot.UnitInfo().getUnits(Players::Self,  sc2::UNIT_TYPEID::TERRAN_BARRACKS);
	const int numRax = static_cast<int>(Rax.size());
	const int numRaxFinished = buildingsFinished(Rax);

	const CUnits Starports = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_STARPORT , sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING }));
	const int numStarport = static_cast<int>(Starports.size());
	const int numStarportFinished = buildingsFinished(Starports);

	const CUnits Factories = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_FACTORY , sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
	const int numFactory = static_cast<int>(Factories.size());
	const int numFactoryFinished = buildingsFinished(Factories);

	const CUnits Engibays = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY);
	const int numEngibays = static_cast<int>(Engibays.size());
	const int numEngibaysFinished = buildingsFinished(Engibays);

	if (m_bot.underAttack() && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).supplyCost)
	{
		//If we are under attack marines have priority
		//std::cout << "Under attack!" << std::endl;
		for (const auto & unit : Rax)
		{
			//Any finished rax
			if (unit->isCompleted())
			{
				//that is idle
				const auto addon = m_bot.UnitInfo().getUnit(unit->getAddOnTag());
				if (unit->isIdle() || (addon && addon->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR && unit->getOrders().size() < 2))
				{

					if (minerals >= 50 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).supplyCost)
					{
						m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_MARINE);
						std::cout << "Marine" << std::endl;
						return;
					}

				}
			}
		}
	}
	const float mineralRate = m_bot.Observation()->GetScore().score_details.collection_rate_minerals;
	const int reactorsFinished = buildingsFinished(m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR));
	if (!m_bot.underAttack() && mineralRate / static_cast<float>(reactorsFinished)<400.f && numBases - numBasesFinished + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) == 0)
	{
		if (minerals > 400)
		{
			m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER), BUILDING, false));
			std::cout << "CC" << std::endl;	
		}
		std::cout << "Need CC" << std::endl;
		return;
	}

	if (supplyCap < 200 && (supplyCap - supply < (numBasesFinished + 2 * numRaxFinished) || (numDepots==0&&minerals>100)))
	{
		//count the building and requested 
		const int numBuildingDepots = numDepots-numDepotsFinished;
		
		if (numBuildingDepots + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < numBasesFinished)
		{
			m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT), BUILDING, false));
			std::cout << "Depot" << std::endl;
			return;
		}
	}
	for (const auto & unit : CommandCenters)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			//Before building a worker, make sure it is a OC
			if (unit->getUnitType() == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER && numRaxFinished > 0)
			{
				if (minerals > 150)
				{
					Micro::SmartAbility(unit, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND, m_bot);
				}
				std::cout << "OC" << std::endl;
				return;
			}

			//Every Base has to build a worker until 66 workers.
			if (m_bot.Observation()->GetFoodWorkers() < 66)
			{
				//m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_SCV), UNIT, false));
				Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_SCV, m_bot);
				std::cout << "TrainSCV" << std::endl;
				return;
			}

		}
	}
	//Refinaries
	int numOfRefinaries = static_cast<int>(m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_REFINERY, false) + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_REFINERY));
	if (gas <= 500 && numRax > 0 && minerals >= 75 && ((m_bot.Observation()->GetFoodWorkers() >= 15 && numOfRefinaries == 0) || (m_bot.Observation()->GetFoodWorkers() >= 21 && numOfRefinaries == 1) || (m_bot.Observation()->GetFoodWorkers() >= 40 && numOfRefinaries <= 3)))
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_REFINERY), BUILDING, false));
		std::cout << "Refinary" << std::endl;
		return;
	}
	//Upgrades techlab
	const CUnits bTechLab = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB);
	std::vector<sc2::UpgradeID> upgrades = m_bot.Observation()->GetUpgrades();
	for (const auto & unit : bTechLab)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			if (std::find(upgrades.begin(),upgrades.end(),sc2::UPGRADE_ID::SHIELDWALL) == upgrades.end())
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_COMBATSHIELD,m_bot);
					}
					std::cout << "CombatShield" << std::endl;
					return;
				}
			}
			else if (std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::STIMPACK) == upgrades.end())
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 100)
				{
					if (minerals >= 100)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_STIMPACK, m_bot);
					}
					std::cout << "Stim" << std::endl;
					return;
				}
			}
			else if (std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::PUNISHERGRENADES) == upgrades.end())
			{
				//If you have enough minerals but not enough gas do not block. That mins could be marines.
				if (gas >= 50)
				{
					if (minerals >= 50)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS,m_bot);
					}
					std::cout << "Concussiv" << std::endl;
					return;
				}
			}
			
		}
	}

	//upgrades
	const CUnits Armories = m_bot.UnitInfo().getUnits(sc2::Unit::Alliance::Self, sc2::UNIT_TYPEID::TERRAN_ARMORY);
	const int numArmories = static_cast<int>(Armories.size());
	const int numArmoriesFinished = buildingsFinished(Armories);

	const int weapon = m_bot.getWeapon();
	const int armor = m_bot.getArmor();
	for (const auto & unit : Engibays)
	{
		if (unit->isCompleted() && unit->isIdle())
		{
			std::vector<sc2::AvailableAbility> abilities = m_bot.Query()->GetAbilitiesForUnit(unit->getUnit_ptr(),true).abilities;
			// Weapons and armor research has consecutive numbers
			//First weapons
			for (sc2::AvailableAbility & ability : abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS)
				{
					const sc2::UpgradeID upgradeID = Util::abilityIDToUpgradeID(ability.ability_id);
					if ((weapon == 0 && gas >= 100) || (weapon == 1 && gas >= 175) || (weapon == 2 && gas >= 250))
					{
						if ((weapon == 0 && minerals >= 100) || (weapon == 1 && minerals >= 175) || (weapon == 2 && minerals >= 250))
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS, m_bot);
						}
						std::cout << "Weapon ++" << std::endl;
						return;
					}
				}
			}
			//Then armor
			for (sc2::AvailableAbility & ability : abilities)
			{
				if (ability.ability_id == sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR)
				{
					const sc2::UpgradeID upgradeID = Util::abilityIDToUpgradeID(ability.ability_id);
					if ((armor == 0 && gas >= 100) || (armor == 1 && gas >= 175) || (armor == 2 && gas >= 250))
					{
						if ((armor == 0 && minerals >= 100) || (armor == 1 && minerals >= 175) || (armor == 2 && minerals >= 250))
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR, m_bot);
						}
						std::cout << "Armor ++" << std::endl;
						return;
					}
				}
			}
		}
	}
	//Starports
	for (const auto & unit : Starports)
	{
		//Any finished Starport
		if (unit->isCompleted())
		{
			//that is idle
			const auto addon = m_bot.UnitInfo().getUnit(unit->getAddOnTag());
			if (unit->isIdle() || unit->getOrders().size() == 1 && addon && addon->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR)
			{
				//if it is only a Starport build addon
				if (!addon)
				{
					//The reactor for the first starport should be with the factory
					for (const auto & factory : Factories)
					{
						if ((factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->getOrders().empty() && factory->getAddOnTag() && !unit->isFlying()) || (factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING && factory->getOrders().empty() && unit->isFlying()))
						{
							Util::swapBuildings(unit, factory, m_bot);
							std::cout << "Swap" << std::endl;
							return;
						}
						else
						{
							//Something went wrong. The Factory has a techlab. Build the reactor yourself.
							if (factory->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORY && factory->getOrders().empty() && (!factory->getAddOnTag() || (m_bot.UnitInfo().getUnit(factory->getAddOnTag())->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)))
							{
								if (minerals >= 50 && gas >= 25)
								{
									Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_REACTOR_STARPORT, m_bot);
									std::cout << "ReactorStarport" << std::endl;
									return;
								}
							}
						}
					}
				}
				else
				{
					const size_t numMedivacs = m_bot.UnitInfo().getUnitTypeCount(sc2::Unit::Alliance::Self, sc2::UNIT_TYPEID::TERRAN_MEDIVAC);
					const size_t numVikings = m_bot.UnitInfo().getUnitTypeCount(sc2::Unit::Alliance::Self, { sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER,sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT });
					if ((!m_vikingRequested && numMedivacs < 12) || (m_vikingRequested && numMedivacs <= numVikings))
					{
						if (gas >= 100 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MEDIVAC).supplyCost)
						{
							if (minerals >= 100)
							{
								m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_MEDIVAC);
							}
							std::cout << "Medivac" << std::endl;
							return;
						}
					}
					else
					{
						if (minerals >= 150 && gas >= 75 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER).supplyCost)
						{
							m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_VIKINGFIGHTER);
							std::cout << "Viking" << std::endl;
							return;
						}
					}
				}
			}
		}
	}
	//Every rax has to build
	const CUnits bReactor = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR);
	for (const auto & unit : Rax)
	{
		//Any finished rax
		if (unit->isCompleted())
		{
			//that is idle
			const auto addon = m_bot.UnitInfo().getUnit(unit->getAddOnTag());
			if (unit->isIdle() || (addon && addon->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR && unit->getOrders().size() == 1))
			{
				//Building the scout has priority
				if (m_scoutRequested)
				{
					if (gas >= 50)
					{
						if (minerals >= 50)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_REAPER,m_bot);
							m_scoutRequested = false;
						}
						std::cout << "Scout" << std::endl;
						return;
					}
				}
				//if it is only a rax build addon
				if (!addon)
				{
					//The second barracks gets a lab
					if (bTechLab.size() == 1 || bReactor.size() == 0)
					{
						if (minerals >= 50 && gas >= 50)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_REACTOR_BARRACKS,m_bot);
							std::cout << "ReactorBarracks" << std::endl;
							return;
						}
					}
					else
					{
						if (minerals >= 50 && gas >= 25)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS,m_bot);
							std::cout << "TechlabBarracks" << std::endl;
							return;
						}
					}
				}
				//If we have an addon
				else
				{
					//Reactor -> double marines
					if (addon->getUnitType() == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR)
					{
						if (minerals >= 50 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARINE).supplyCost)
						{
							m_bot.Actions()->UnitCommand(unit->getUnit_ptr(), sc2::ABILITY_ID::TRAIN_MARINE);
							std::cout << "Marine" << std::endl;
							return;
						}
					}
					//techlab ->marauder
					{
						if (minerals >= 100 && gas >= 25 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_MARAUDER).supplyCost)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_MARAUDER,m_bot);
							std::cout << "Marauder" << std::endl;
							return;
						}
					}
				}
			}
		}
	}
	for (const auto & unit : Factories)
	{
		//Any finished Factory
		if (unit->isCompleted() && !unit->isFlying())
		{
			//that is idle
			if (unit->isIdle())
			{
				//if it is only a factory build addon
				if (!unit->getAddOnTag())
				{
					//Build a reactor for the first starport
					if (numStarport==0 || numStarport==1 && numStarportFinished==0)
					{
						if (minerals >= 50 && gas >= 50 && numStarport == 1)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_REACTOR_FACTORY,m_bot);
							std::cout << "ReactorFactory" << std::endl;
							return;
						}
					}
					else if (Starports.size()>0 && (Starports.front()->getAddOnTag() || (Starports.front()->getOrders().size()>0 && Starports.front()->getOrders().front().ability_id==sc2::ABILITY_ID::BUILD_REACTOR)))
					{
						if (minerals >= 50 && gas >= 25)
						{
							Micro::SmartAbility(unit, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY,m_bot);
							std::cout << "TechlabFactory" << std::endl;
							return;
						}
					}
				}
				else if (m_bot.UnitInfo().getUnit(unit->getAddOnTag())->getUnitType().ToType() == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)
				{
					const int numWidowMines = static_cast<int>(m_bot.UnitInfo().getUnitTypeCount(Players::Self, { sc2::UNIT_TYPEID::TERRAN_WIDOWMINE,sc2::UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED }));
					if (numWidowMines==0 && minerals >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).mineralCost && gas >= m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).gasCost && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_WIDOWMINE).supplyCost)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_WIDOWMINE,m_bot);
						std::cout << "WM" << std::endl;
						return;
					}
					if (minerals >= 150 && gas >= 125 && supply <= 200 - m_bot.Data(sc2::UNIT_TYPEID::TERRAN_SIEGETANK).supplyCost)
					{
						Micro::SmartAbility(unit, sc2::ABILITY_ID::TRAIN_SIEGETANK,m_bot);
						std::cout << "Tank" << std::endl;
						return;
					}
				}
			}
		}
	}
	if (minerals >= 100 && numBases == 2)// m_bot.GetPlayerRace(Players::Enemy) != sc2::Race::Terran &&
	{
		const CUnits Bunker = m_bot.UnitInfo().getUnits(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER );
		if (Bunker.size() + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BUNKER) < 1)
		{
			m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BUNKER), BUILDING, false));
		}
	}

	//Engineering bay
	//We start with one. We want to built it after the starport has finished
	if (minerals >= 125
		&& ((numStarportFinished == 1 && numEngibays + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY) == 0)
		|| (numEngibays + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY) == 1 && numArmoriesFinished == 1)))
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY), BUILDING, false));
		std::cout << "Engineering Bay" << std::endl;
		return;
	}

	//Armory
	//Needed for upgrades after +1
	if (weapon >= 1 && armor >= 0 && gas >= 100 && numArmories + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_ARMORY) == 0)
	{
		//A armory needs 46 seconds to build.
		//+1 weapons needs 114 seconds.
		//So only after 68/114 seconds we need to build the armory
		//Since it always takes a while we do it 10sec earlier
		float maxProgress = 0.0f;
		for (const auto & engi : Engibays)
		{
			if (engi->getOrders().empty())
			{
				maxProgress = 1.0f;
				break;
			}
			const float progress = engi->getOrders().front().progress;
			if (maxProgress < progress)
			{
				maxProgress = progress;
			}
		}
		if (maxProgress >= 58.0f / 114.0f)
		{
			if (minerals >= 150)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_ARMORY), BUILDING, false));
			}
			return;
		}
	}

	//Factories. For now just one.
	if (numBases> 1 && minerals >= 150 && gas >= 100 && numFactory + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_FACTORY) == 0 && bReactor.size()>0)
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_FACTORY), BUILDING, false));
		std::cout << "Factory" << std::endl;
		return;
	}
	//Starports. For now just one.
	if (numBases> 1 && minerals >= 150 && gas >= 100 && numFactoryFinished>0 && numStarport + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_STARPORT) ==0)
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_STARPORT), BUILDING, false));
		std::cout << "Starport" << std::endl;
		return;
	}
	//On one base save a little to get an expansion
	if (numBases == 1)
	{
		if (true)//m_bot.GetPlayerRace(Players::Enemy) == sc2::Race::Protoss)
		{
			if (numDepotsFinished > 0 && minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 2)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
				std::cout << "Barracks" << std::endl;
				return;
			}
		}
		else
		{
			if (numDepotsFinished > 0 && minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
				std::cout << "Barracks" << std::endl;
				return;
			}
		}
	}
	//On two bases let the starport start first
	else
	{
		if (numRax==1 || numStarport>0)
		{
			if (minerals >= 150 && numRax + howOftenQueued(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 3 * numBases - 4 && numRax <= 9)
			{
				m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_BARRACKS), BUILDING, false));
				std::cout << "Barracks" << std::endl;
				return;
			}
		}
	}
	//expanding
	if (minerals >= 400 && numBases-numBasesFinished+howOftenQueued(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)==0)
	{
		m_newQueue.push_back(BuildOrderItem(BuildType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER), BUILDING, false));
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

void ProductionManager::requestVikings()
{
	m_vikingRequested = true;
}

void ProductionManager::requestScan()
{
	++m_scansRequested;
}

void ProductionManager::usedScan(int i)
{
	//if we needed one...
	if (m_scansRequested > 1+i)
	{
		m_scansRequested-=i;
	}
	else
	{
		m_scansRequested = 1;
	}
}

int ProductionManager::buildingsFinished(const CUnits units) const
{
	int numBuildingsFinished = 0;
	for (const auto & unit : units)
	{
		if (unit->isCompleted())
		{
			numBuildingsFinished++;
		}
	}
	return numBuildingsFinished;
}

int ProductionManager::howOftenQueued(sc2::UnitTypeID type)
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
			//ss << sc2::UnitTypeToName(unit.unit_type) << " " << unit.build_progress << "\n";
		}
	}
	for (const auto & b : m_newQueue)
	{
		ss << "\n" << sc2::UnitTypeToName(b.m_type.getUnitTypeID());
	}
	Drawing::drawTextScreen(m_bot,sc2::Point2D(0.01f, 0.01f), ss.str(), sc2::Colors::Yellow);
}