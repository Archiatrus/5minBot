#pragma once
#include "Common.h"
#include "CUnit.h"

class CCBot;

namespace WorkerJobs
{
    enum { Minerals, Gas, Build, Combat, Idle, Repair, Move, Scout, None, Num };
}

class WorkerData
{
    CCBot & m_bot;

    CUnits        m_workers;
    CUnits         m_depots;
    std::map<int, int>                  m_workerJobCount;
    std::map<CUnit_ptr, int>    m_workerJobMap;
    std::map<CUnit_ptr, int>    m_refineryWorkerCount;
    std::map<CUnit_ptr, int>    m_depotWorkerCount;
    std::map<CUnit_ptr,CUnit_ptr>  m_workerRefineryMap;
    std::map<CUnit_ptr,CUnit_ptr>  m_workerDepotMap;
	std::map<sc2::Tag, CUnits > m_repair_map;

    void clearPreviousJob(const CUnit_ptr unit);

public:

    WorkerData(CCBot & bot);

    void    workerDestroyed(const CUnit_ptr unit);
    void    updateAllWorkerData();
    void    updateWorker(const CUnit_ptr unit);
    void    setWorkerJob(const CUnit_ptr unit, int job, const CUnit_ptr jobUnit = nullptr);
    void    drawDepotDebugInfo();
    size_t  getNumWorkers() const;
    int     getWorkerJobCount(int job) const;
    int     getNumAssignedWorkers(const CUnit_ptr unit);
    int     getWorkerJob(const CUnit_ptr unit) const;
    const CUnit_ptr getMineralToMine(const CUnit_ptr unit) const;
    const CUnit_ptr getWorkerDepot(const CUnit_ptr unit) const;
    const char * getJobCode(const CUnit_ptr unit);
    const CUnits & getWorkers() const;
	const CUnits getMineralWorkers() const;
	const CUnits getGasWorkers() const;
	const size_t isBeingRepairedNum(const CUnit_ptr unit) const;
	void checkRepairedBuildings();
};