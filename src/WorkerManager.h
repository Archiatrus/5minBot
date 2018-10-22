#pragma once

#include "WorkerData.h"

class Building;
class CCBot;

class WorkerManager
{
	CCBot & m_bot;

	mutable WorkerData	m_workerData;
	const sc2::Unit *	m_previousClosestWorker;

	void	setMineralWorker(const CUnit_ptr unit);
	
	void	handleIdleWorkers();
	void	handleGasWorkers();
	void		handleMineralWorkers();
	void		handleRepairWorkers();

public:

	WorkerManager(CCBot & bot);

	void		onStart();
	void		onFrame();

	

	void	finishedWithWorker(const CUnit_ptr unit);
	void	drawResourceDebugInfo();
	void	drawWorkerInformation();
	void	setScoutWorker(const CUnit_ptr worker);
	void	setCombatWorker(const CUnit_ptr worker);
	void	setBuildingWorker(const CUnit_ptr worker, Building & b);
	void	setRepairWorker(const CUnit_ptr worker,const CUnit_ptr unitToRepair);
    void	setRepairWorker(const CUnit_ptr unitToRepair, size_t numWorkers = 1);
	void	stopRepairing(const CUnit_ptr worker);

	int	getNumMineralWorkers();
	int	getNumGasWorkers();
	bool	isWorkerScout(const CUnit_ptr worker) const;
	bool	isRepairWorker(const CUnit_ptr worker) const;
	bool	isFree(const CUnit_ptr worker) const;
	bool	isBuilder(const CUnit_ptr worker) const;

	const CUnit_ptr getBuilder(Building & b,bool setJobAsBuilder = true) const;
	const CUnits	getMineralWorkers() const;
	int getNumAssignedWorkers(const CUnit_ptr unit) const;
	const CUnits getCombatWorkers() const;
	const CUnit_ptr getClosestDepot(const CUnit_ptr worker) const;
	const CUnit_ptr getGasWorker(const CUnit_ptr refinery) const;
	const CUnit_ptr getClosestMineralWorkerTo(const sc2::Point2D & pos) const;

	const CUnit_ptr getClosestCombatWorkerTo(const sc2::Point2D & pos) const;

    size_t isBeingRepairedNum(const CUnit_ptr unit) const;
};

