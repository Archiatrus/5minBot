#include "WorkerManager.h"
#include "Micro.h"
#include "CCBot.h"
#include "Util.h"
#include "sc2api/sc2_score.h"
#include "Building.h"

WorkerManager::WorkerManager(CCBot & bot)
	: m_bot(bot)
	, m_workerData(bot)
{
	m_previousClosestWorker = nullptr;
}

void WorkerManager::onStart()
{
}

void WorkerManager::onFrame()
{
	m_workerData.updateAllWorkerData();
	handleGasWorkers();
	handleMineralWorkers();
	handleRepairWorkers();
	handleIdleWorkers();

	drawResourceDebugInfo();
	drawWorkerInformation();

	m_workerData.drawDepotDebugInfo();
}

void WorkerManager::setRepairWorker(const CUnit_ptr unitToRepair, size_t numWorkers)
{
    if (isBeingRepairedNum(unitToRepair) < numWorkers)
	{
		const CUnit_ptr worker = getClosestMineralWorkerTo(unitToRepair->getPos());
		if (worker)
		{
			m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
		}
		// Repair has higher priority than combat
		else
		{
			const CUnit_ptr combatWorker = getClosestCombatWorkerTo(unitToRepair->getPos());
			if (combatWorker)
			{
				m_workerData.setWorkerJob(combatWorker, WorkerJobs::Repair, unitToRepair);
			}
		}
	}
}

void WorkerManager::setRepairWorker(const CUnit_ptr worker, const CUnit_ptr unitToRepair)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
}

/*
void WorkerManager::stopRepairing(const CUnit_ptr worker)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
}
*/

void WorkerManager::handleGasWorkers()
{
	// We want first 13 workers on minerals
	const int minNumMinWorkers = 13;
	int numMinWorker = m_workerData.getWorkerJobCount(WorkerJobs::Minerals);
	int numGasWorker = m_workerData.getWorkerJobCount(WorkerJobs::Gas);
	if (numMinWorker < minNumMinWorkers && numGasWorker >= 1)
	{
		for (const auto & gasWorker : m_workerData.getGasWorkers())
		{
			if (gasWorker->getOrders().empty() || gasWorker->getOrders().front().ability_id != sc2::ABILITY_ID::HARVEST_RETURN)
			{
				setMineralWorker(gasWorker);
				numMinWorker++;
				numGasWorker--;
				if (numMinWorker > minNumMinWorkers || (numGasWorker == 1 && numMinWorker > 5))
				{
					return;
				}
			}
		}
	}
	else
	{
		// for each unit we have
		for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
		{
			// if that unit is a refinery
			if (unit->IsRefineryType() && unit->isCompleted() && unit->getVespeneContents() > 0 )
			{
				// get the number of workers currently assigned to it
				int numAssigned = m_workerData.getNumAssignedWorkers(unit);
				// check if a liberator is targeting the area
				bool enemyTooClose = false;
				for (const auto & effect : m_bot.Observation()->GetEffects())
				{
					if (effect.effect_id == sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHING) || effect.effect_id == sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHING))
					{
						if (Util::DistSq(effect.positions.front(), unit->getPos()) < 100.0f)
						{
							enemyTooClose = true;
							break;
						}
					}
				}
				// if it's less than we want it to be, fill 'er up
				if (!enemyTooClose && numAssigned<3 && numMinWorker>minNumMinWorkers)
				{
					auto gasWorker = getGasWorker(unit);
					if (gasWorker)
					{
						m_workerData.setWorkerJob(gasWorker, WorkerJobs::Gas, unit);
					}
				}
			}
		}
	}
}
void WorkerManager::handleMineralWorkers()
{
	const CUnits CommandCenters = m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes());
	// for each unit we have
	if (!m_bot.underAttack())
	{
		for (const auto & unit : CommandCenters)
		{
			// if that unit is a townhall
			if (unit->isCompleted())
			{
				// get the number of workers currently assigned to it
				const int numAssigned = unit->getAssignedHarvesters();
				// and the max number
				const int numMaxAssigned = unit->getIdealHarvesters();
				// if it's too much try to find another base
				if (numAssigned > numMaxAssigned)
				{
					for (const auto & unit_next : CommandCenters)
					{
						// if that unit is finished
						if (unit_next->isCompleted())
						{
							int numAssigned_next = unit_next->getAssignedHarvesters();
							int numMaxAssigned_next = unit_next->getIdealHarvesters();
							if (numAssigned_next < numMaxAssigned_next)
							{
								auto transfer_worker = getClosestMineralWorkerTo(unit->getPos());  // THIS WILL PROBABLY CALL THE SAME WORKER OVER AND OVER AGAIN UNTIL HE IS FAR ENOUGH AWAY :(
								CUnits minerals = m_bot.Bases().getBaseLocation(unit_next->getPos())->getMinerals();
								CUnit_ptr closestMineral = nullptr;
								float minDist = std::numeric_limits<float>::max();
								for (auto mineral : minerals)
								{
									if (mineral->getDisplayType() == sc2::Unit::DisplayType::Snapshot)
									{
										mineral = m_bot.GetUnit(mineral->getTag());
									}
									if (mineral->isAlive() && mineral->getDisplayType() == sc2::Unit::DisplayType::Visible && mineral->getMineralContents() > 200)
									{
										const float dist = Util::DistSq(mineral->getPos(), unit_next->getPos());
										if (minDist > dist)
										{
											minDist = dist;
											closestMineral = mineral;
										}
									}
								}
								if (closestMineral && transfer_worker)
								{
									m_workerData.setWorkerJob(transfer_worker, WorkerJobs::Minerals, closestMineral);
									return;
								}
							}
						}
					}
					// if it's really too much try to find anything
					if (numAssigned > 1.5f*numMaxAssigned)
					{
						auto transfer_worker = getClosestMineralWorkerTo(unit->getPos());  // THIS WILL PROBABLY CALL THE SAME WORKER OVER AND OVER AGAIN UNTIL HE IS FAR ENOUGH AWAY :(
						const CUnit_ptr newBase = m_bot.Bases().getBaseLocation(m_bot.Bases().getNewestExpansion(Players::Self))->getTownHall();
						sc2::Point2D pos;
						if (newBase && !newBase->isCompleted())
						{
							pos = newBase->getPos();
						}
						else
						{
							pos = m_bot.Bases().getNextExpansion(Players::Self);
						}
						CUnit_ptr closestMineral = nullptr;
						float minDist = std::numeric_limits<float>::max();
						for (auto mineral : m_bot.UnitInfo().getUnits(Players::Neutral, Util::getMineralTypes()))
						{
							if (mineral->getDisplayType() == sc2::Unit::DisplayType::Snapshot)
							{
								mineral = m_bot.GetUnit(mineral->getTag());
							}
							if (mineral->isAlive() && mineral->getDisplayType() == sc2::Unit::DisplayType::Visible && mineral->getMineralContents() > 200)
							{
								const float dist = Util::DistSq(mineral->getPos(), pos);
								if (minDist > dist)
								{
									minDist = dist;
									closestMineral = mineral;
								}
							}
						}
						if (closestMineral && transfer_worker)
						{
							m_workerData.setWorkerJob(transfer_worker, WorkerJobs::Minerals, closestMineral);
							m_bot.Production().needCC();
							return;
						}
					}
				}
			}
		}
	}
	else
	{
		const CUnits bunkers = m_bot.UnitInfo().getUnits(Players::Self, std::vector<sc2::UNIT_TYPEID>{ sc2::UNIT_TYPEID::TERRAN_BUNKER, sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS });
		const CUnits enemies = m_bot.UnitInfo().getUnits(Players::Enemy);
		for (const auto & worker : getMineralWorkers())
		{
			if (!worker->getBuffs().empty() && (worker->getBuffs().front().ToType() == sc2::BUFF_ID::CARRYMINERALFIELDMINERALS || worker->getBuffs().front().ToType() == sc2::BUFF_ID::CARRYHIGHYIELDMINERALFIELDMINERALS))
			{
				continue;
			}
			bool enemyTooClose = false;
			for (const auto & enemy : enemies)
			{
				if (enemy->isVisible() && Util::DistSq(enemy->getPos(), worker->getPos()) < 100.0f)
				{
					enemyTooClose = true;
					break;
				}
			}
			if (!enemyTooClose)
			{
				for (const auto & effect : m_bot.Observation()->GetEffects())
				{
					if (effect.effect_id == sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHING) || effect.effect_id == sc2::EffectID(sc2::EFFECT_ID::LIBERATORMORPHING))
					{
						if (Util::DistSq(effect.positions.front(), worker->getPos()) < 100.0f)
						{
							enemyTooClose = true;
							break;
						}
					}
				}
			}
			if (enemyTooClose)
			{
				float distToBunker = 900.0f;
				CUnit_ptr closestBunker = nullptr;
				for (const auto & bunker : bunkers)
				{
					float dist = Util::DistSq(bunker->getPos(), worker->getPos());
					if (!closestBunker || distToBunker > dist)
					{
						closestBunker = bunker;
						distToBunker = dist;
					}
				}
				CUnit_ptr mineralPatch = nullptr;
				if (closestBunker)
				{
					if (distToBunker > 400.0f)
					{
						mineralPatch = Util::getClostestMineral(closestBunker->getPos(), m_bot);
					}
				}
				else
				{
					mineralPatch = Util::getClostestMineral(m_bot.Bases().getPlayerStartingBaseLocation(Players::Self)->getCenterOfBase(), m_bot);
				}
				if (mineralPatch)
				{
					m_workerData.setWorkerJob(worker, WorkerJobs::Minerals, mineralPatch);
				}
			}
		}
	}
}

void WorkerManager::handleIdleWorkers()
{
    // for each of our workers
    for (const auto & worker : m_workerData.getWorkers())
    {
        if (!worker) { continue; }

        // if it's a scout or combat, don't handle it here
        if (m_workerData.getWorkerJob(worker) == WorkerJobs::Scout || m_workerData.getWorkerJob(worker) == WorkerJobs::Combat || m_workerData.getWorkerJob(worker) == WorkerJobs::Repair)
        {
            continue;
        }

        // if it is idle
        if (worker->isIdle() || m_workerData.getWorkerJob(worker) == WorkerJobs::Idle)
        {
            setMineralWorker(worker);
        }
    }
}

void WorkerManager::handleRepairWorkers()
{
	const CUnits buildings = m_bot.UnitInfo().getUnits(Players::Self);
	if (m_workerData.getMineralWorkers().size() > 5)
	{
		for (const auto & b : buildings)
		{
			if (b->isBuilding())
			{
				if (b->isCompleted() && b->getHealth() < b->getHealthMax())
				{
					if (b->isType(sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS))
					{
						int numNearWorkers = 0;
						for (const auto & worker : getMineralWorkers())
						{
							if (Util::DistSq(b->getPos(), worker->getPos()) < 100.0f)
							{
								++numNearWorkers;
							}
						}
						int numOfWorkers = std::max(6, numNearWorkers);
						setRepairWorker(b, numOfWorkers);
					}
					if (b->isType(sc2::UNIT_TYPEID::TERRAN_BUNKER) || b->isType(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET))
					{
						setRepairWorker(b, 6);
					}
					else if (b->isType(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND) || b->isType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT))
					{
						setRepairWorker(b, 2);
					}
					else
					{
						setRepairWorker(b);
					}
				}
				else if (b->isType(sc2::UNIT_TYPEID::TERRAN_BUNKER) && m_bot.underAttack())
				{
					const size_t bases = m_bot.UnitInfo().getUnitTypeCount(Players::Self, Util::getTownHallTypes());
					if (bases <= 1)
					{
						// Drawing::cout{} << "We are under attack so prepair repair workers!" << std::endl;
						setRepairWorker(b, 3);
					}
				}
			}
		}
	}
	m_workerData.checkRepairedBuildings();
}

const CUnit_ptr WorkerManager::getClosestMineralWorkerTo(const sc2::Point2D & pos) const
{
    CUnit_ptr closestMineralWorker = nullptr;
    double closestDist = std::numeric_limits<double>::max();

	// for each of our workers
	for (const auto & worker : m_workerData.getWorkers())
	{
		if (!worker) { continue; }

		// if it is a mineral worker
		if (m_workerData.getWorkerJob(worker) == WorkerJobs::Minerals && worker->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_MULE)
        {
			// do not use worker if he carries minerals
			if (worker->getOrders().empty() || worker->getOrders().front().ability_id != sc2::ABILITY_ID::HARVEST_RETURN)
			{
				double dist = Util::DistSq(worker->getPos(), pos);

				if (!closestMineralWorker || dist < closestDist)
				{
					closestMineralWorker = worker;
					closestDist = dist;
				}
			}
		}
	}

	return closestMineralWorker;
}

const CUnit_ptr WorkerManager::getClosestCombatWorkerTo(const sc2::Point2D & pos) const
{
	CUnit_ptr closestMineralWorker = nullptr;
	double closestDist = std::numeric_limits<double>::max();

	// for each of our workers
	for (const auto & worker : m_workerData.getWorkers())
	{
		if (!worker) { continue; }

		// if it is a mineral worker
		if (m_workerData.getWorkerJob(worker) == WorkerJobs::Combat && worker->getUnitType().ToType() != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			// do not use worker if he carries minerals
			double dist = Util::Dist(worker->getPos(), pos);

			if (!closestMineralWorker || dist < closestDist)
			{
				closestMineralWorker = worker;
				closestDist = dist;
			}
		}
	}

	return closestMineralWorker;
}

size_t WorkerManager::isBeingRepairedNum(const CUnit_ptr unit) const
{
	return m_workerData.isBeingRepairedNum(unit);
}


// set a worker to mine minerals
void WorkerManager::setMineralWorker(const CUnit_ptr unit)
{
	// check if there is a mineral available to send the worker to
	// First we want to go to the closest base
	const CUnit_ptr mineralPatch = Util::getClostestMineral(unit->getPos(), m_bot);
	if (mineralPatch)
	{
		m_workerData.setWorkerJob(unit, WorkerJobs::Minerals, mineralPatch);
	}
}

/*
const CUnit_ptr WorkerManager::getClosestDepot(const CUnit_ptr worker) const
{
    CUnit_ptr closestDepot = nullptr;
    double closestDistance = std::numeric_limits<double>::max();

    for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self, Util::getTownHallTypes()))
    {
        if (!unit) { continue; }

        if (unit->isCompleted())
        {
            double distance = Util::DistSq(unit->getPos(), worker->getPos());
            if (!closestDepot || distance < closestDistance)
            {
                closestDepot = unit;
                closestDistance = distance;
            }
        }
    }

    return closestDepot;
}
*/

// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(const CUnit_ptr unit)
{
	m_workerData.setWorkerJob(unit, WorkerJobs::Idle);
}

const CUnit_ptr WorkerManager::getGasWorker(const CUnit_ptr refinery) const
{
    return getClosestMineralWorkerTo(refinery->getPos());
}

/*
void WorkerManager::setBuildingWorker(const CUnit_ptr worker, Building & b)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Build, b.buildingUnit);
}
*/

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
const CUnit_ptr WorkerManager::getBuilder(Building & b, bool setJobAsBuilder) const
{
    const CUnit_ptr builderWorker = getClosestMineralWorkerTo(b.finalPosition);

	// if the worker exists (one may not have been found in rare cases)
	if (builderWorker && setJobAsBuilder)
	{
		m_workerData.setWorkerJob(builderWorker, WorkerJobs::Build, b.builderUnit);
	}

	return builderWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(const CUnit_ptr worker)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Scout);
}

void WorkerManager::setCombatWorker(const CUnit_ptr worker)
{
	m_workerData.setWorkerJob(worker, WorkerJobs::Combat);
}

void WorkerManager::drawResourceDebugInfo()
{
    if (true)
    {
        return;
    }

    for (const auto & worker : m_workerData.getWorkers())
    {
        if (!worker) { continue; }

        Drawing::drawText(m_bot, worker->getPos(), m_workerData.getJobCode(worker));

        auto depot = m_workerData.getWorkerDepot(worker);
        if (depot)
        {
            Drawing::drawLine(m_bot, worker->getPos(), depot->getPos());
        }
    }
}

void WorkerManager::drawWorkerInformation()
{
	if (true)
	{
		return;
	}

	std::stringstream ss;
	ss << "Workers: " << m_workerData.getWorkers().size() << "\n";

	for (const auto & workerTag : m_workerData.getWorkers())
	{
		ss << m_workerData.getJobCode(workerTag) << " " << workerTag << "\n";
	}

	Drawing::drawTextScreen(m_bot, sc2::Point2D(0.75f, 0.2f), ss.str());
}

bool WorkerManager::isFree(const CUnit_ptr worker) const
{
	return worker->getUnitType() != sc2::UNIT_TYPEID::TERRAN_MULE && (m_workerData.getWorkerJob(worker) == WorkerJobs::Minerals || m_workerData.getWorkerJob(worker) == WorkerJobs::Idle);
}

/*
bool WorkerManager::isWorkerScout(const CUnit_ptr worker) const
{
	return (m_workerData.getWorkerJob(worker) == WorkerJobs::Scout);
}
*/

bool WorkerManager::isRepairWorker(const CUnit_ptr worker) const
{
	return (m_workerData.getWorkerJob(worker) == WorkerJobs::Repair);
}


bool WorkerManager::isBuilder(const CUnit_ptr worker) const
{
	return (m_workerData.getWorkerJob(worker) == WorkerJobs::Build);
}

/*
int WorkerManager::getNumMineralWorkers()
{
	return m_workerData.getWorkerJobCount(WorkerJobs::Minerals);
}
*/

/*
int WorkerManager::getNumGasWorkers()
{
	return m_workerData.getWorkerJobCount(WorkerJobs::Gas);
}
*/

const CUnits WorkerManager::getMineralWorkers() const
{
	return m_workerData.getMineralWorkers();
}

int WorkerManager::getNumAssignedWorkers(const CUnit_ptr unit) const
{
	return  m_workerData.getNumAssignedWorkers(unit);
}

const CUnits WorkerManager::getCombatWorkers() const
{
	return m_workerData.getCombatWorkers();
}
