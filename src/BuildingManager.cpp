#include "Common.h"
#include "BuildingManager.h"
#include "Micro.h"
#include "CCBot.h"
#include "Util.h"
#include "Drawing.h"


const int frames = 60;

BuildingManager::BuildingManager(CCBot & bot)
	: m_bot(bot)
	, m_buildingPlacer(bot)
	, m_debugMode(false)
	, m_reservedMinerals(0)
	, m_reservedGas(0)
{
}

void BuildingManager::onStart()
{
	m_buildingPlacer.onStart();
}

// gets called every frame from GameCommander
void BuildingManager::onFrame()
{
	reservedResourcesCheck();

	m_buildingPlacer.onFrame();

	validateWorkersAndBuildings();		  // check to see if assigned workers have died en route or while constructing
	assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'
	constructAssignedBuildings();		   // for each planned building, if the worker isn't constructing, send the command
	requestGuards();						// for new expansion we need to send guards to secure the area
	checkForStartedConstruction();		  // check to see if any buildings have started construction and update data structures
	checkForDeadTerranBuilders();		   // if we are terran and a building is under construction without a worker, assign a new one
	checkForCompletedBuildings();		   // check to see if any buildings have completed and update data structures

	drawBuildingInformation();
}

void BuildingManager::reservedResourcesCheck()
{
	bool shouldReset = true;
	for (const auto & b : m_buildings)
	{
		if (!b.buildingUnit)
		{
			shouldReset = false;
		}
	}
	if (shouldReset)
	{
		resetFreeMinerals();
		resetFreeGas();
	}
}

bool BuildingManager::isBeingBuilt(sc2::UnitTypeID type)
{
	for (const auto & b : m_buildings)
	{
		if (b.m_type == type)
		{
			return true;
		}
	}

	return false;
}

// STEP 1: DO BOOK KEEPING ON WORKERS WHICH MAY HAVE DIED
void BuildingManager::validateWorkersAndBuildings()
{
	std::vector<Building> toRemove;

	// find any buildings which have become obsolete
	for (const auto & b : m_buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			continue;
		}

		auto buildingUnit = b.buildingUnit;

		if (!buildingUnit || buildingUnit->getHealthPoints() <= 0)
		{
			toRemove.push_back(b);
		}
		// Cancel buildings if they are about to be destroyed
		else if (buildingUnit->getHealthPoints() / buildingUnit->getHealthPointsMax() < 0.1f && buildingUnit->getBuildProgress() >buildingUnit->getHealthPoints() / buildingUnit->getHealthPointsMax())
		{
			Micro::SmartAbility(buildingUnit, sc2::ABILITY_ID::CANCEL_BUILDINPROGRESS, m_bot);
			toRemove.push_back(b);
		}
	}

	removeBuildings(toRemove);
}

// STEP 2: ASSIGN WORKERS TO BUILDINGS WITHOUT THEM
void BuildingManager::assignWorkersToUnassignedBuildings()
{
	// for each building that doesn't have a builder, assign one
	for (Building & b : m_buildings)
	{
		if (b.status != BuildingStatus::Unassigned)
		{
			continue;
		}

		BOT_ASSERT(b.builderUnit == nullptr, "Error: Tried to assign a builder to a building that already had one ");

		if (m_debugMode) { printf("Assigning Worker To: %s", sc2::UnitTypeToName(b.m_type)); }

		// grab a worker unit from WorkerManager which is closest to this final position
		sc2::Point2D testLocation = getBuildingLocation(b);
		if (!m_bot.Map().isValid(testLocation) && testLocation != sc2::Point2D{ 0.0f, 0.0f })
		{
			continue;
		}

		b.finalPosition = testLocation;

		// grab the worker unit from WorkerManager which is closest to this final position
		const CUnit_ptr builderUnit = m_bot.Workers().getBuilder(b);
		b.builderUnit = builderUnit;
		if (!b.builderUnit)
		{
			continue;
		}

		// reserve this building's space
		if (b.m_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS || b.m_type == sc2::UNIT_TYPEID::TERRAN_FACTORY || b.m_type == sc2::UNIT_TYPEID::TERRAN_STARPORT)
		{
			m_buildingPlacer.reserveTiles(static_cast<int>(b.finalPosition.x), static_cast<int>(b.finalPosition.y), Util::GetUnitTypeWidth(b.m_type, m_bot)+2, Util::GetUnitTypeHeight(b.m_type, m_bot));
		}
		else
		{
			m_buildingPlacer.reserveTiles(static_cast<int>(b.finalPosition.x), static_cast<int>(b.finalPosition.y), Util::GetUnitTypeWidth(b.m_type, m_bot), Util::GetUnitTypeHeight(b.m_type, m_bot));
		}

		b.status = BuildingStatus::Assigned;
	}
}

// STEP 3: ISSUE CONSTRUCTION ORDERS TO ASSIGN BUILDINGS AS NEEDED
void BuildingManager::constructAssignedBuildings()
{
	std::vector<Building> toRemove;
	for (auto & b : m_buildings)
	{
		if (b.status != BuildingStatus::Assigned)
		{
			continue;
		}

		sc2::AbilityID buildAbility = m_bot.Data(b.m_type).buildAbility;
		std::shared_ptr<CUnit> builderUnit = b.builderUnit;
		// If the builder died on its way remove this building
		if (builderUnit && !builderUnit->isAlive())
		{
			toRemove.push_back(b);
			continue;
		}
		// if we are under attack don't start buildings
		if (m_bot.underAttack())
		{
			if (b.m_type == sc2::UNIT_TYPEID::TERRAN_BUNKER || b.m_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
			{
				// We still want to build bunker and supply depots
			}
			else if (m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_BUNKER))
			{
				// We can take an exe if the bunker is finished.
			}
			else
			{
				toRemove.push_back(b);
				continue;
			}
		}
		bool isConstructing = false;

		// if we're zerg and the builder unit is null, we assume it morphed into the building
		if (m_bot.GetPlayerRace(Players::Self) == sc2::Race::Zerg)
		{
			if (!builderUnit)
			{
				isConstructing = true;
			}
		}
		else
		{
			BOT_ASSERT(builderUnit, "null builder unit");
			isConstructing = (builderUnit->getOrders().size() > 0) && (builderUnit->getOrders().front().ability_id == buildAbility);
		}
		// if that worker is not currently constructing
		if (!isConstructing)
		{
			// if we haven't explored the build position, go there
			if (!isBuildingPositionExplored(b))
			{
				Micro::SmartMove(builderUnit, b.finalPosition, m_bot);
			}
			// if this is not the first time we've sent this guy to build this
			// it must be the case that something was in the way of building
			else if (b.buildCommandGiven)
			{
				Micro::SmartBuild(b.builderUnit, b.m_type, b.finalPosition, m_bot);
			}
			else
			{
				// if it's a refinery, the build command has to be on the geyser unit tag
				if (Util::IsRefineryType(b.m_type))
				{
					// first we find the geyser at the desired location
					CUnit_ptr geyser = nullptr;
					for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Neutral, Util::getGeyserTypes()))
					{
						if (Util::Dist(b.finalPosition, unit->getPos()) < 3)
						{
							geyser = unit;
							break;
						}
					}

					if (geyser)
					{
						Micro::SmartBuildTarget(b.builderUnit, b.m_type, geyser, m_bot);
					}
					else
					{
						std::cout << "WARNING: NO VALID GEYSER UNIT FOUND TO BUILD ON, SKIPPING REFINERY\n";
					}
				}
				// if it's not a refinery, we build right on the position
				else
				{
					if (m_bot.Query()->Placement(m_bot.Data(b.m_type).buildAbility, b.finalPosition))
					{
						Micro::SmartBuild(b.builderUnit, b.m_type, b.finalPosition, m_bot);
					}
				}

				// set the flag to true
				b.buildCommandGiven = true;
			}
		}
	}
	removeBuildings(toRemove);
}
// Step 3.5: Request guards for new expansions
void BuildingManager::requestGuards()
{
	for (const auto & b : m_buildings)
	{
		if (b.m_type.ToType() == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
		{
			// We need guards when the build order is issued bot there is no building yet
			if (b.status == BuildingStatus::Assigned)
			{
				m_bot.requestGuards(true);
				// We usually only build one CC at a time
				return;
			}
		}
	}
}


// STEP 4: UPDATE DATA STRUCTURES FOR BUILDINGS STARTING CONSTRUCTION
void BuildingManager::checkForStartedConstruction()
{
	// for each building unit which is being constructed
	for (auto buildingStarted : m_bot.UnitInfo().getUnits(Players::Self))
	{
		// filter out units which aren't buildings under construction
		if (!buildingStarted->isBuilding() || buildingStarted->getBuildProgress() == 0.0f || buildingStarted->getBuildProgress() == 1.0f)
		{
			continue;
		}

		// check all our building status objects to see if we have a match and if we do, update it

		for (auto & b : m_buildings)
		{
			if (b.status != BuildingStatus::Assigned)
			{
				continue;
			}

			// check if the positions match
			float dx = b.finalPosition.x - buildingStarted->getPos().x;
			float dy = b.finalPosition.y - buildingStarted->getPos().y;

			if (dx*dx + dy*dy < 1)
			{
				if (b.buildingUnit != nullptr)
				{
					std::cout << "Building mis-match somehow\n";
				}

				// the resources should now be spent, so unreserve them
				m_reservedMinerals -= Util::GetUnitTypeMineralPrice(buildingStarted->getUnitType(), m_bot);
				m_reservedGas      -= Util::GetUnitTypeGasPrice(buildingStarted->getUnitType(), m_bot);

				// flag it as started and set the buildingUnit
				b.underConstruction = true;
				b.buildingUnit = buildingStarted;

				// if we are zerg, the buildingUnit now becomes nullptr since it's destroyed
				if (m_bot.GetPlayerRace(Players::Self) == sc2::Race::Zerg)
				{
					b.builderUnit = nullptr;
				}
				else if (m_bot.GetPlayerRace(Players::Self) == sc2::Race::Protoss)
				{
					m_bot.Workers().finishedWithWorker(b.builderUnit);
					b.builderUnit = nullptr;
				}

				// put it in the under construction vector
				b.status = BuildingStatus::UnderConstruction;
				// Starting with the construction we do not need any guards anymore since the rally point defaults to newest base
				if (b.m_type.ToType() == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
				{
					m_bot.requestGuards(false);
				}
				// free this space
				// but not if it is an starport or factory.
				// this whole part should actually only be done if the building is destroyed.
				if (b.m_type.ToType() != sc2::UNIT_TYPEID::TERRAN_FACTORY && b.m_type.ToType() != sc2::UNIT_TYPEID::TERRAN_STARPORT)
				{
					m_buildingPlacer.freeTiles(static_cast<int>(b.finalPosition.x), static_cast<int>(b.finalPosition.y), Util::GetUnitTypeWidth(b.m_type, m_bot), Util::GetUnitTypeHeight(b.m_type, m_bot));
				}

				// only one building will match
				break;
			}
		}
	}
}

// STEP 5: IF WE ARE TERRAN, THIS MATTERS, SO: LOL
void BuildingManager::checkForDeadTerranBuilders()
{
	std::vector<Building> toRemove;
	// At this point all buildings should have a builder. If not remove them
	for (auto & b : m_buildings)
	{
		if (b.status == BuildingStatus::UnderConstruction)
		{
			if (!m_bot.underAttack() || b.m_type == sc2::UNIT_TYPEID::TERRAN_BUNKER || b.m_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
			{
				if (!b.builderUnit || !b.builderUnit->isAlive())
				{
					const CUnit_ptr builderUnit = m_bot.Workers().getBuilder(b);
					if (builderUnit)
					{
						b.builderUnit = builderUnit;
						Micro::SmartRightClick(builderUnit, b.buildingUnit, m_bot);
					}
				}
				else if (b.builderUnit && (b.builderUnit->getOrders().empty() || b.builderUnit->getOrders().front().ability_id == sc2::ABILITY_ID::HARVEST_GATHER))
				{
					Micro::SmartRightClick(b.builderUnit, b.buildingUnit, m_bot);
				}
			}
		}
		if (b.status == BuildingStatus::Assigned)
		{
			if (!b.builderUnit->isAlive() || b.builderUnit->getOrders().empty() || !(isBuildingOrder(b.builderUnit->getOrders().front().ability_id) || b.builderUnit->getOrders().front().ability_id == sc2::ABILITY_ID::MOVE))
			{
				if (b.lastOrderFrame > frames)  // We give the worker 10 frames to move
				{
					toRemove.push_back(b);
					m_reservedMinerals -= Util::GetUnitTypeMineralPrice(b.m_type, m_bot);
					m_reservedGas -= Util::GetUnitTypeGasPrice(b.m_type, m_bot);
				}
				else
				{
					b.lastOrderFrame++;
				}
			}
		}
	}
	removeBuildings(toRemove);
}

// STEP 6: CHECK FOR COMPLETED BUILDINGS
void BuildingManager::checkForCompletedBuildings()
{
	std::vector<Building> toRemove;

	// for each of our buildings under construction
	for (auto & b : m_buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			continue;
		}

		// if the unit has completed
		if (b.buildingUnit->getBuildProgress() == 1.0f)
		{
			// if we are terran, give the worker back to worker manager
			if (m_bot.GetPlayerRace(Players::Self) == sc2::Race::Terran)
			{
				if (b.m_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
				{
					Micro::SmartAbility(b.buildingUnit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER, m_bot);
				}
				m_bot.Workers().finishedWithWorker(b.builderUnit);
			}
			// remove this unit from the under construction vector
			toRemove.push_back(b);
		}
	}

	removeBuildings(toRemove);
}

// add a new building to be constructed
void BuildingManager::addBuildingTask(const sc2::UnitTypeID & type, const sc2::Point2D & desiredPosition)
{
	m_reservedMinerals  += Util::GetUnitTypeMineralPrice(type, m_bot);
	m_reservedGas		+= Util::GetUnitTypeGasPrice(type, m_bot);

	Building b(type, desiredPosition);
	b.status = BuildingStatus::Unassigned;

	// with an empty building task queue there shouldn't be any reserved tiles.
	if (m_buildings.empty())
	{
		bool canReset = true;
		// Well there are. The ones reserved for addons...
		const CUnits flyingBuildings = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
		if (flyingBuildings.empty())
		{
			const CUnits addonBuildings = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>({ sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_STARPORT , sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORY, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING }));
			for (const auto & addonBuilding : addonBuildings)
			{
				if (!addonBuilding->getAddOnTag())
				{
					canReset = false;
					break;
				}
			}
		}
		if (canReset)
		{
			m_buildingPlacer.freeTiles();
		}
	}
	m_buildings.push_back(b);
}

bool BuildingManager::isBuildingPositionExplored(const Building & b) const
{
	return m_bot.Map().isExplored(b.finalPosition);
}


char BuildingManager::getBuildingWorkerCode(const Building & b) const
{
	return b.builderUnit == nullptr ? 'X' : 'W';
}

int BuildingManager::getReservedMinerals()
{
	return m_reservedMinerals;
}

int BuildingManager::getReservedGas()
{
	return m_reservedGas;
}


void BuildingManager::resetFreeMinerals()
{
	m_reservedMinerals = 0;
}

void BuildingManager::resetFreeGas()
{
	m_reservedGas = 0;
}

void BuildingManager::drawBuildingInformation()
{
	m_buildingPlacer.drawReservedTiles();

	if (!m_bot.Config().DrawBuildingInfo)
	{
		return;
	}

	std::stringstream ss;
	ss << "Building Information " << m_buildings.size() << "\n\n\n";

	for (const auto & b : m_buildings)
	{
		std::stringstream dss;

		if (b.builderUnit)
		{
			dss << "\n\nBuilder: " << b.builderUnit->getTag() << "\n";
		}

		if (b.buildingUnit)
		{
			dss << "Building: " << b.buildingUnit->getTag() << "\n" << b.buildingUnit->getBuildProgress();
			Drawing::drawText(m_bot, b.buildingUnit->getPos(), dss.str());
		}



		if (b.status == BuildingStatus::Unassigned)
		{
			ss << "Unassigned " << sc2::UnitTypeToName(b.m_type) << "    " << getBuildingWorkerCode(b) << "\n";
		}
		else if (b.status == BuildingStatus::Assigned)
		{
			ss << "Assigned " << sc2::UnitTypeToName(b.m_type) << "    " << b.builderUnit->getTag() << " " << getBuildingWorkerCode(b) << " (" << b.finalPosition.x << "," << b.finalPosition.y << ")\n";

			float x1 = b.finalPosition.x;
			float y1 = b.finalPosition.y;
			float x2 = b.finalPosition.x + Util::GetUnitTypeWidth(b.m_type, m_bot);
			float y2 = b.finalPosition.y + Util::GetUnitTypeHeight(b.m_type, m_bot);

			Drawing::drawSquare(m_bot, x1, y1, x2, y2, sc2::Colors::Red);
			// m_bot.Map().drawLine(b.finalPosition, m_bot.GetUnit(b.builderUnitTag)->pos, sc2::Colors::Yellow);
		}
		else if (b.status == BuildingStatus::UnderConstruction)
		{
			ss << "Constructing " << sc2::UnitTypeToName(b.m_type) << "    " << b.builderUnit->getTag() << " " << b.buildingUnit->getTag() << " " << getBuildingWorkerCode(b) << "\n";
		}
	}

	Drawing::drawTextScreen(m_bot, sc2::Point2D(0.05f, 0.05f), ss.str());
}

std::vector<sc2::UnitTypeID> BuildingManager::buildingsQueued() const
{
	std::vector<sc2::UnitTypeID> buildingsQueued;

	for (const auto & b : m_buildings)
	{
		if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
		{
			buildingsQueued.push_back(b.m_type);
		}
	}

	return buildingsQueued;
}

int BuildingManager::getNumBuildingsQueued(sc2::UnitTypeID type) const
{
	int numBuildingsQueued = 0;

	for (const auto & b : m_buildings)
	{
		if (b.m_type == type && (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned))
		{
			numBuildingsQueued++;
		}
	}

	return numBuildingsQueued;
}

sc2::Point2D BuildingManager::getBuildingLocation(const Building & b)
{
	if (Util::IsRefineryType(b.m_type))
	{
		return m_buildingPlacer.getRefineryPosition();
	}

	if (Util::IsTownHallType(b.m_type))
	{
		if (m_bot.Query()->Placement(sc2::ABILITY_ID::BUILD_COMMANDCENTER, b.desiredPosition))
		{
			return b.desiredPosition;
		}
		return m_buildingPlacer.getTownHallLocationNear(b);
	}

	if (b.m_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
	{
		sc2::Point2D testPoint = m_bot.Map().getWallPositionDepot();
		if (testPoint != sc2::Point2D(0, 0))
		{
			return testPoint;
		}
	}
	if (b.m_type == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)
	{
		if (m_bot.Query()->Placement(sc2::ABILITY_ID::BUILD_MISSILETURRET, b.desiredPosition))
		{
			return b.desiredPosition;
		}
		return m_buildingPlacer.getBuildLocationNear(b, 0);
	}
	// get a position within our region
	return m_buildingPlacer.getBuildLocationNear(b, m_bot.Config().BuildingSpacing);
}

void BuildingManager::removeBuildings(const std::vector<Building> & toRemove)
{
	for (const auto & b : toRemove)
	{
		const auto & it = std::find(m_buildings.begin(), m_buildings.end(), b);

		if (it != m_buildings.end())
		{
			if (b.builderUnit && b.builderUnit->isAlive())
			{
				m_bot.Workers().finishedWithWorker(b.builderUnit);
			}
			m_buildingPlacer.freeTiles(static_cast<int>(b.finalPosition.x), static_cast<int>(b.finalPosition.y), Util::GetUnitTypeWidth(b.m_type, m_bot), Util::GetUnitTypeHeight(b.m_type, m_bot));
			m_buildings.erase(it);
		}
	}
}

bool BuildingManager::isBuildingOrder(sc2::ABILITY_ID id)
{
	switch (id)
	{
	case sc2::ABILITY_ID::BUILD_ARMORY: return true;
	case sc2::ABILITY_ID::BUILD_ASSIMILATOR: return true;
	case sc2::ABILITY_ID::BUILD_BANELINGNEST: return true;
	case sc2::ABILITY_ID::BUILD_BARRACKS: return true;
	case sc2::ABILITY_ID::BUILD_BUNKER: return true;
	case sc2::ABILITY_ID::BUILD_COMMANDCENTER: return true;
	case sc2::ABILITY_ID::BUILD_CREEPTUMOR: return true;
	case sc2::ABILITY_ID::BUILD_CREEPTUMOR_QUEEN: return true;
	case sc2::ABILITY_ID::BUILD_CREEPTUMOR_TUMOR: return true;
	case sc2::ABILITY_ID::BUILD_CYBERNETICSCORE: return true;
	case sc2::ABILITY_ID::BUILD_DARKSHRINE: return true;
	case sc2::ABILITY_ID::BUILD_ENGINEERINGBAY: return true;
	case sc2::ABILITY_ID::BUILD_EVOLUTIONCHAMBER: return true;
	case sc2::ABILITY_ID::BUILD_EXTRACTOR: return true;
	case sc2::ABILITY_ID::BUILD_FACTORY: return true;
	case sc2::ABILITY_ID::BUILD_FLEETBEACON: return true;
	case sc2::ABILITY_ID::BUILD_FORGE: return true;
	case sc2::ABILITY_ID::BUILD_FUSIONCORE: return true;
	case sc2::ABILITY_ID::BUILD_GATEWAY: return true;
	case sc2::ABILITY_ID::BUILD_GHOSTACADEMY: return true;
	case sc2::ABILITY_ID::BUILD_HATCHERY: return true;
	case sc2::ABILITY_ID::BUILD_HYDRALISKDEN: return true;
	case sc2::ABILITY_ID::BUILD_INFESTATIONPIT: return true;
	case sc2::ABILITY_ID::BUILD_INTERCEPTORS: return true;
	case sc2::ABILITY_ID::BUILD_MISSILETURRET: return true;
	case sc2::ABILITY_ID::BUILD_NEXUS: return true;
	case sc2::ABILITY_ID::BUILD_NUKE: return true;
	case sc2::ABILITY_ID::BUILD_NYDUSNETWORK: return true;
	case sc2::ABILITY_ID::BUILD_NYDUSWORM: return true;
	case sc2::ABILITY_ID::BUILD_PHOTONCANNON: return true;
	case sc2::ABILITY_ID::BUILD_PYLON: return true;
	case sc2::ABILITY_ID::BUILD_REACTOR: return true;
	case sc2::ABILITY_ID::BUILD_REACTOR_BARRACKS: return true;
	case sc2::ABILITY_ID::BUILD_REACTOR_FACTORY: return true;
	case sc2::ABILITY_ID::BUILD_REACTOR_STARPORT: return true;
	case sc2::ABILITY_ID::BUILD_REFINERY: return true;
	case sc2::ABILITY_ID::BUILD_ROACHWARREN: return true;
	case sc2::ABILITY_ID::BUILD_ROBOTICSBAY: return true;
	case sc2::ABILITY_ID::BUILD_ROBOTICSFACILITY: return true;
	case sc2::ABILITY_ID::BUILD_SENSORTOWER: return true;
	case sc2::ABILITY_ID::BUILD_SPAWNINGPOOL: return true;
	case sc2::ABILITY_ID::BUILD_SPINECRAWLER: return true;
	case sc2::ABILITY_ID::BUILD_SPIRE: return true;
	case sc2::ABILITY_ID::BUILD_SPORECRAWLER: return true;
	case sc2::ABILITY_ID::BUILD_STARGATE: return true;
	case sc2::ABILITY_ID::BUILD_STARPORT: return true;
	case sc2::ABILITY_ID::BUILD_STASISTRAP: return true;
	case sc2::ABILITY_ID::BUILD_SUPPLYDEPOT: return true;
	case sc2::ABILITY_ID::BUILD_TECHLAB: return true;
	case sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS: return true;
	case sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY: return true;
	case sc2::ABILITY_ID::BUILD_TECHLAB_STARPORT: return true;
	case sc2::ABILITY_ID::BUILD_TEMPLARARCHIVE: return true;
	case sc2::ABILITY_ID::BUILD_TWILIGHTCOUNCIL: return true;
	case sc2::ABILITY_ID::BUILD_ULTRALISKCAVERN: return true;
	}
	return false;
}
