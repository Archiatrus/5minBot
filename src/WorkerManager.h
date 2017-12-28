#pragma once

#include "WorkerData.h"

class Building;
class CCBot;

class WorkerManager
{
    CCBot & m_bot;

    mutable WorkerData  m_workerData;
    const sc2::Unit *   m_previousClosestWorker;

    void        setMineralWorker(const sc2::Unit * unit);
    
    void        handleIdleWorkers();
    void        handleGasWorkers();
	void		handleMineralWorkers();
    void        handleRepairWorkers();

public:

    WorkerManager(CCBot & bot);

    void        onStart();
    void        onFrame();

	

    void        finishedWithWorker(const sc2::Unit * unit);
    void        drawResourceDebugInfo();
    void        drawWorkerInformation();
    void        setScoutWorker(const sc2::Unit * worker);
    void        setCombatWorker(const sc2::Unit * worker);
    void        setBuildingWorker(const sc2::Unit * worker, Building & b);
    void        setRepairWorker(const sc2::Unit * worker,const sc2::Unit * unitToRepair);
	void		setRepairWorker(const sc2::Unit * unitToRepair, int numWorkers = 1);
    void        stopRepairing(const sc2::Unit * worker);

    int         getNumMineralWorkers();
    int         getNumGasWorkers();
    bool        isWorkerScout(const sc2::Unit * worker) const;
	bool		isRepairWorker(const sc2::Unit * worker) const;
    bool        isFree(const sc2::Unit * worker) const;
    bool        isBuilder(const sc2::Unit * worker) const;

    const sc2::Unit * getBuilder(Building & b,bool setJobAsBuilder = true) const;
	std::vector <const sc2::Unit* >  getMineralWorkers() const;
	int getNumAssignedWorkers(const sc2::Unit * unit) const;
    const sc2::Unit * getClosestDepot(const sc2::Unit * worker) const;
    const sc2::Unit * getGasWorker(const sc2::Unit * refinery) const;
    const sc2::Unit * getClosestMineralWorkerTo(const sc2::Point2D & pos) const;

	const sc2::Unit * getClosestCombatWorkerTo(const sc2::Point2D & pos) const;

	const size_t WorkerManager::isBeingRepairedNum(const sc2::Unit * unit) const;
};

