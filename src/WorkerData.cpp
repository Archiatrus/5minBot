#include "WorkerData.h"
#include "Micro.h"
#include "Util.h"
#include "CCBot.h"
#include <iostream>
#include <sstream>

WorkerData::WorkerData(CCBot & bot)
	: m_bot(bot)
{
	for (int i=0; i < WorkerJobs::Num; ++i)
	{
		m_workerJobCount[i] = 0;
	}
}

void WorkerData::updateAllWorkerData()
{
    // check all our units and add new workers if we find them
	for (const auto & unit : m_bot.UnitInfo().getUnits(Players::Self, Util::getWorkerTypes()))
	{
		updateWorker(unit);
	}

    // for each of our Workers
    for (const auto & worker : getWorkers())
    {
        // if it's idle
        if (getWorkerJob(worker) == WorkerJobs::None)
        {
            setWorkerJob(worker, WorkerJobs::Idle);
        }

        // TODO: If it's a gas worker whose refinery has been destroyed, set to minerals
    }

    // remove any worker units which no longer exist in the game
    CUnits workersDestroyed;
    for (const auto & worker : getWorkers())
    {
        // TODO: for now skip gas workers because they disappear inside refineries, this is annoying
        if (!worker || !worker->isAlive() || (m_bot.Observation()->GetGameLoop() != worker->getLastSeenGameLoop() && getWorkerJob(worker) != WorkerJobs::Gas))
        {
            workersDestroyed.push_back(worker);
        }
    }

    for (const auto & worker : workersDestroyed)
    {
        workerDestroyed(worker);
    }
}

void WorkerData::workerDestroyed(const CUnit_ptr unit)
{
    clearPreviousJob(unit);
	m_workers.erase(std::remove_if(m_workers.begin(), m_workers.end(), [unit](CUnit_ptr & newUnit) {return unit->getTag() == newUnit->getTag(); }),m_workers.end());
}

void WorkerData::updateWorker(const CUnit_ptr unit)
{
    if (std::find_if(m_workers.begin(), m_workers.end(), [unit](CUnit_ptr & newUnit) {return unit->getTag() == newUnit->getTag(); }) == m_workers.end() && unit->isAlive())
    {
        m_workers.push_back(unit);
        m_workerJobMap[unit] = WorkerJobs::None;
    }
}

void WorkerData::setWorkerJob(const CUnit_ptr unit, int job, const CUnit_ptr jobUnit)
{
    clearPreviousJob(unit);
    m_workerJobMap[unit] = job;
    m_workerJobCount[job]++;

    if (job == WorkerJobs::Minerals)
    {
        // if we haven't assigned anything to this depot yet, set its worker count to 0
        if (m_depotWorkerCount.find(jobUnit) == m_depotWorkerCount.end())
        {
            m_depotWorkerCount[jobUnit] = 0;
        }

        // add the depot to our set of depots
        m_depots.push_back(jobUnit);

        // increase the worker count of this depot
        m_workerDepotMap[unit] = jobUnit;
        m_depotWorkerCount[jobUnit]++;

        // find the mineral to mine and mine it
		//WHY??? WE HAVE THIS ALREADY?
        //const CUnit_ptr mineralToMine = getMineralToMine(unit);
        
		if (jobUnit->getDisplayType() == sc2::Unit::DisplayType::Visible && jobUnit->isAlive())
		{
			//Micro::SmartMove(unit, jobUnit->pos, m_bot);
			Micro::SmartAbility(unit, sc2::ABILITY_ID::HARVEST_GATHER,jobUnit,m_bot);
			//Micro::SmartRightClick(unit, jobUnit, m_bot,true);
		}
		else
		{
			const CUnit_ptr mineralToMine = getMineralToMine(unit);
			Micro::SmartRightClick(unit, mineralToMine, m_bot);
		}
    }
    else if (job == WorkerJobs::Gas)
    {
        // if we haven't assigned any workers to this refinery yet set count to 0
        if (m_refineryWorkerCount.find(jobUnit) == m_refineryWorkerCount.end())
        {
            m_refineryWorkerCount[jobUnit] = 0;
        }

        // increase the count of workers assigned to this refinery
        m_refineryWorkerCount[jobUnit] += 1;
        m_workerRefineryMap[unit] = jobUnit;

        // right click the refinery to start harvesting
        Micro::SmartRightClick(unit, jobUnit, m_bot);
    }
    else if (job == WorkerJobs::Repair)
    {
		m_repair_map[jobUnit->getTag()].push_back(unit);
        Micro::SmartRepair(unit, jobUnit, m_bot);
    }
    else if (job == WorkerJobs::Scout)
    {

	}
	else if (job == WorkerJobs::Build)
	{

	}
}

void WorkerData::clearPreviousJob(const CUnit_ptr unit)
{
	const int previousJob = getWorkerJob(unit);
	m_workerJobCount[previousJob]--;

	if (previousJob == WorkerJobs::Minerals)
	{
		// remove one worker from the count of the depot this worker was assigned to
		m_depotWorkerCount[m_workerDepotMap[unit]]--;
		m_workerDepotMap.erase(unit);
	}
	else if (previousJob == WorkerJobs::Gas)
	{
		m_refineryWorkerCount[m_workerRefineryMap[unit]]--;
		m_workerRefineryMap.erase(unit);
	}
	else if (previousJob == WorkerJobs::Build)
	{

	}
	else if (previousJob == WorkerJobs::Repair)
	{
		
	}
	else if (previousJob == WorkerJobs::Move)
	{

	}

	m_workerJobMap.erase(unit);
}

size_t WorkerData::getNumWorkers() const
{
	return m_workers.size();
}

int WorkerData::getWorkerJobCount(int job) const
{
	return m_workerJobCount.at(job);
}

int WorkerData::getWorkerJob(const CUnit_ptr unit) const
{
	auto it = m_workerJobMap.find(unit);

	if (it != m_workerJobMap.end())
	{
		return it->second;
	}

	return WorkerJobs::None;
}

const CUnit_ptr WorkerData::getMineralToMine(const CUnit_ptr unit) const
{
    CUnit_ptr bestMineral = nullptr;
    double bestDist = 100000;

    for (const auto & mineral : m_bot.UnitInfo().getUnits(Players::Neutral,Util::getMineralTypes()))
    {
        if (!mineral->isMineral()||!mineral->isAlive()) continue;

        double dist = Util::Dist(mineral->getPos(), unit->getPos());

		if (dist < bestDist)
		{
			bestMineral = mineral;
			bestDist = dist;
		}
	}

	return bestMineral;
}

const CUnit_ptr WorkerData::getWorkerDepot(const CUnit_ptr unit) const
{
	auto it = m_workerDepotMap.find(unit);

	if (it != m_workerDepotMap.end())
	{
		return it->second;
	}

	return nullptr;
}

int WorkerData::getNumAssignedWorkers(const CUnit_ptr unit)
{
    if (unit->isTownHall())
    {
        auto it = m_depotWorkerCount.find(unit);

        // if there is an entry, return it
        if (it != m_depotWorkerCount.end())
        {
            return it->second;
        }
    }
    else if (unit->getUnitType()==sc2::UNIT_TYPEID::TERRAN_REFINERY)
    {
        auto it = m_refineryWorkerCount.find(unit);

        // if there is an entry, return it
        if (it != m_refineryWorkerCount.end())
        {
            return it->second;
        }
        // otherwise, we are only calling this on completed refineries, so set it
        else
        {
            m_refineryWorkerCount[unit] = 0;
        }
    }

    // when all else fails, return 0
    return 0;
}

const char * WorkerData::getJobCode(const CUnit_ptr unit)
{
	const int j = getWorkerJob(unit);

	if (j == WorkerJobs::Build)	 return "B";
	if (j == WorkerJobs::Combat)	return "C";
	if (j == WorkerJobs::None)	  return "N";
	if (j == WorkerJobs::Gas)	   return "G";
	if (j == WorkerJobs::Idle)	  return "I";
	if (j == WorkerJobs::Minerals)  return "M";
	if (j == WorkerJobs::Repair)	return "R";
	if (j == WorkerJobs::Move)	  return "O";
	if (j == WorkerJobs::Scout)	 return "S";
	return "X";
}

void WorkerData::drawDepotDebugInfo()
{
	return;/*
	for (const auto & depot: m_depots)
	{
		std::stringstream ss;
		ss << "Workers: " << getNumAssignedWorkers(depot);

        Drawing::drawText(m_bot,depot->getPos(), ss.str());
    }*/
}

const CUnits & WorkerData::getWorkers() const
{
	return m_workers;
}

const CUnits WorkerData::getMineralWorkers() const
{
	std::vector<CUnit_ptr> mineralWorkers;
	for (const auto & unitjob : m_workerJobMap)
	{
		if (unitjob.first && unitjob.second == WorkerJobs::Minerals)
		{
			mineralWorkers.push_back(unitjob.first);
		}
	}
	return mineralWorkers;
}
const CUnits WorkerData::getGasWorkers() const
{
	std::vector<CUnit_ptr> gasWorkers;
	for (const auto & unitjob : m_workerJobMap)
	{
		if (unitjob.second == WorkerJobs::Gas)
		{
			gasWorkers.push_back(unitjob.first);
		}
	}
	return gasWorkers;
}

const size_t WorkerData::isBeingRepairedNum(const CUnit_ptr unit) const
{
	if (m_repair_map.find(unit->getTag()) == m_repair_map.end())
	{
		return 0;
	}
	return m_repair_map.at(unit->getTag()).size();
}

void WorkerData::checkRepairedBuildings()
{
	if (m_repair_map.empty())
	{
		return;
	}
	std::vector<sc2::Tag> targetsToDelete;
	for (auto & m : m_repair_map)
	{
		CUnit_ptr target = m_bot.UnitInfo().getUnit(m.first);
		if (!target || !target->isAlive() || target->getHealth() == target->getHealthMax())
		{
			targetsToDelete.push_back(m.first);
			for (const auto & worker : m.second)
			{
				clearPreviousJob(worker);
			}
			continue;
		}
		CUnits & workers = m.second;
		CUnits deadWorker;
		for (const auto & w : workers)
		{
			if (!w->isAlive())
			{
				deadWorker.push_back(w);
				continue;
			}
			if (w->getOrders().empty() || w->getOrders().front().target_unit_tag != m.first)
			{
				Micro::SmartAbility(w, sc2::ABILITY_ID::EFFECT_REPAIR, target,m_bot);
			}
		}
		for (const auto & deadW : deadWorker)
		{
			workers.erase(std::remove(workers.begin(), workers.end(), deadW), workers.end());
		}
	}
	for (const auto & t : targetsToDelete)
	{
		m_repair_map.erase(t);
	}
}