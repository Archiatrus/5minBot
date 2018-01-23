#include "WorkerManager.h"
#include "Micro.h"
#include "CCBot.h"
#include "Util.h"
#include "Building.h"

WorkerManager::WorkerManager(CCBot & bot)
    : m_bot         (bot)
    , m_workerData  (bot)
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

void WorkerManager::setRepairWorker(const sc2::Unit * unitToRepair,int numWorkers)
{
	auto test = isBeingRepairedNum(unitToRepair);
	if (isBeingRepairedNum(unitToRepair)<numWorkers)
	{
		const sc2::Unit * worker = getClosestMineralWorkerTo(unitToRepair->pos);
		if (worker)
		{
			m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
		}
		//Repair has higher priority than combat
		else
		{
			const sc2::Unit * worker = getClosestCombatWorkerTo(unitToRepair->pos);
			if (worker)
			{
				m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
			}
		}
	}
}

void WorkerManager::setRepairWorker(const sc2::Unit * worker, const sc2::Unit * unitToRepair)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(const sc2::Unit * worker)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Idle);
}

void WorkerManager::handleGasWorkers()
{

	//We want first 13 workers on minerals
	const int minNumMinWorkers = 13;
	int numMinWorker = m_workerData.getWorkerJobCount(WorkerJobs::Minerals);
	int numGasWorker = m_workerData.getWorkerJobCount(WorkerJobs::Gas);
	if (numMinWorker < minNumMinWorkers && numGasWorker > 1)
	{
		for (const auto & gasWorker : m_workerData.getGasWorkers())
		{
			if (gasWorker->orders.empty() || gasWorker->orders[0].ability_id != sc2::ABILITY_ID::HARVEST_RETURN)
			{
				setMineralWorker(gasWorker);
				numMinWorker++;
				numGasWorker--;
				if (numMinWorker > minNumMinWorkers || numGasWorker==1)
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
			if (Util::IsRefinery(unit) && Util::IsCompleted(unit) && unit->vespene_contents > 0 )
			{
				// get the number of workers currently assigned to it
				int numAssigned = m_workerData.getNumAssignedWorkers(unit);


				// if it's less than we want it to be, fill 'er up
				if (numAssigned<3 && numMinWorker>minNumMinWorkers)
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
	const sc2::Units CommandCenters = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER , sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND , sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS }));
	// for each unit we have
	for (const auto & unit : CommandCenters)
	{
		// if that unit is a townhall
		if (Util::IsCompleted(unit))
		{
			// get the number of workers currently assigned to it
			int numAssigned = unit->assigned_harvesters;
			// and the max number
			int numMaxAssigned = unit->ideal_harvesters;
			// if it's too much try to find another base
			if (numAssigned > numMaxAssigned)
			{

				for (const auto & unit_next : CommandCenters)
				{
					// if that unit is finished
					if (Util::IsCompleted(unit_next))
					{
						int numAssigned_next = unit_next->assigned_harvesters;
						int numMaxAssigned_next = unit_next->ideal_harvesters;
						if (numAssigned_next<numMaxAssigned_next)
						{
							auto transfer_worker = getClosestMineralWorkerTo(unit->pos); //THIS WILL PROBABLY CALL THE SAME WORKER OVER AND OVER AGAIN UNTIL HE IS FAR ENOUGH AWAY :(
							const sc2::Unit * mineralPatch = Util::getClostestMineral(unit_next->pos, m_bot);
							if (mineralPatch)
							{
								//Micro::SmartMove(unit, unit_next->pos, m_bot);
								m_workerData.setWorkerJob(transfer_worker, WorkerJobs::Minerals, mineralPatch);
								return;
							}
						}
					}
				}
				//Only do it for one townhall at a time. Otherwise there might be problems?!
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
        if (m_workerData.getWorkerJob(worker) == WorkerJobs::Scout || m_workerData.getWorkerJob(worker) == WorkerJobs::Combat)
        {
            continue;
        }

        // if it is idle
        if (Util::IsIdle(worker) || m_workerData.getWorkerJob(worker) == WorkerJobs::Idle)
        {
            setMineralWorker(worker);
        }
    }
}

void WorkerManager::handleRepairWorkers()
{
	const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit( sc2::UNIT_TYPEID::TERRAN_BUNKER ));
	for (const auto & b : Bunker)
	{
		if (b->build_progress==1.0f && b->health < b->health_max)
		{
			setRepairWorker(b,6);
		}
	}
	m_workerData.checkRepairedBuildings();
}

const sc2::Unit * WorkerManager::getClosestMineralWorkerTo(const sc2::Point2D & pos) const
{
    const sc2::Unit * closestMineralWorker = nullptr;
    double closestDist = std::numeric_limits<double>::max();

    // for each of our workers
	for (const auto & worker : m_workerData.getWorkers())
	{
		if (!worker) { continue; }

		// if it is a mineral worker
		if (m_workerData.getWorkerJob(worker) == WorkerJobs::Minerals && worker->unit_type != sc2::UNIT_TYPEID::TERRAN_MULE)
        {
			//do not use worker if he carries minerals
			if (worker->orders.empty() || worker->orders[0].ability_id!=sc2::ABILITY_ID::HARVEST_RETURN)
			{
				double dist = Util::Dist(worker->pos, pos);

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

const sc2::Unit * WorkerManager::getClosestCombatWorkerTo(const sc2::Point2D & pos) const
{
	const sc2::Unit * closestMineralWorker = nullptr;
	double closestDist = std::numeric_limits<double>::max();

	// for each of our workers
	for (const auto & worker : m_workerData.getWorkers())
	{
		if (!worker) { continue; }

		// if it is a mineral worker
		if (m_workerData.getWorkerJob(worker) == WorkerJobs::Combat && worker->unit_type != sc2::UNIT_TYPEID::TERRAN_MULE)
		{
			//do not use worker if he carries minerals
			double dist = Util::Dist(worker->pos, pos);

			if (!closestMineralWorker || dist < closestDist)
			{
				closestMineralWorker = worker;
				closestDist = dist;
			}
		}
	}

	return closestMineralWorker;
}

const size_t WorkerManager::isBeingRepairedNum(const sc2::Unit * unit) const
{
	return m_workerData.isBeingRepairedNum(unit);
}


// set a worker to mine minerals
void WorkerManager::setMineralWorker(const sc2::Unit * unit)
{
    // check if there is a mineral available to send the worker to
	// First we want to go to the closest base
	const sc2::Unit * mineralPatch = Util::getClostestMineral(unit->pos, m_bot);
	if (mineralPatch)
	{
		m_workerData.setWorkerJob(unit, WorkerJobs::Minerals, mineralPatch);
	}
}

const sc2::Unit * WorkerManager::getClosestDepot(const sc2::Unit * worker) const
{
    const sc2::Unit * closestDepot = nullptr;
    double closestDistance = std::numeric_limits<double>::max();

    for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
    {
        if (!unit) { continue; }

        if (Util::IsTownHall(unit) && Util::IsCompleted(unit))
        {
            double distance = Util::DistSq(unit->pos, worker->pos);
            if (!closestDepot || distance < closestDistance)
            {
                closestDepot = unit;
                closestDistance = distance;
            }
        }
    }

    return closestDepot;
}


// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(const sc2::Unit * unit)
{
	m_workerData.setWorkerJob(unit, WorkerJobs::Idle);
}

const sc2::Unit * WorkerManager::getGasWorker(const sc2::Unit * refinery) const
{
    return getClosestMineralWorkerTo(refinery->pos);
}

void WorkerManager::setBuildingWorker(const sc2::Unit * worker, Building & b)
{
    m_workerData.setWorkerJob(worker, WorkerJobs::Build, b.buildingUnit);
}

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
const sc2::Unit * WorkerManager::getBuilder(Building & b, bool setJobAsBuilder) const
{
    const sc2::Unit * builderWorker = getClosestMineralWorkerTo(b.finalPosition);

    // if the worker exists (one may not have been found in rare cases)
    if (builderWorker && setJobAsBuilder)
    {
        m_workerData.setWorkerJob(builderWorker, WorkerJobs::Build, b.builderUnit);
    }

    return builderWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(const sc2::Unit * workerTag)
{
    m_workerData.setWorkerJob(workerTag, WorkerJobs::Scout);
}

void WorkerManager::setCombatWorker(const sc2::Unit * workerTag)
{
    m_workerData.setWorkerJob(workerTag, WorkerJobs::Combat);
}

void WorkerManager::drawResourceDebugInfo()
{
    if (!m_bot.Config().DrawResourceInfo)
    {
        return;
    }

    for (const auto & worker : m_workerData.getWorkers())
    {
        if (!worker) { continue; }

        Drawing::drawText(m_bot,worker->pos, m_workerData.getJobCode(worker));

        auto depot = m_workerData.getWorkerDepot(worker);
        if (depot)
        {
            Drawing::drawLine(m_bot,worker->pos, depot->pos);
        }
    }
}

void WorkerManager::drawWorkerInformation()
{
    if (!m_bot.Config().DrawWorkerInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Workers: " << m_workerData.getWorkers().size() << "\n";

    int yspace = 0;

    for (const auto & workerTag : m_workerData.getWorkers())
    {
        ss << m_workerData.getJobCode(workerTag) << " " << workerTag << "\n";
    }

    Drawing::drawTextScreen(m_bot,sc2::Point2D(0.75f, 0.2f), ss.str());
}

bool WorkerManager::isFree(const sc2::Unit * worker) const
{
    return m_workerData.getWorkerJob(worker) == WorkerJobs::Minerals || m_workerData.getWorkerJob(worker) == WorkerJobs::Idle;
}

bool WorkerManager::isWorkerScout(const sc2::Unit * worker) const
{
    return (m_workerData.getWorkerJob(worker) == WorkerJobs::Scout);
}

bool WorkerManager::isRepairWorker(const sc2::Unit * worker) const
{
	return (m_workerData.getWorkerJob(worker) == WorkerJobs::Repair);
}


bool WorkerManager::isBuilder(const sc2::Unit * worker) const
{
    return (m_workerData.getWorkerJob(worker) == WorkerJobs::Build);
}

int WorkerManager::getNumMineralWorkers()
{
    return m_workerData.getWorkerJobCount(WorkerJobs::Minerals);
}

int WorkerManager::getNumGasWorkers()
{
    return m_workerData.getWorkerJobCount(WorkerJobs::Gas);

}

std::vector<const sc2::Unit *> WorkerManager::getMineralWorkers() const
{
	return m_workerData.getMineralWorkers();
}

int WorkerManager::getNumAssignedWorkers(const sc2::Unit * unit) const
{
	return  m_workerData.getNumAssignedWorkers(unit);
}