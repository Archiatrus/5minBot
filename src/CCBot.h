#pragma once

#include "sc2api/sc2_api.h"

#include "MapTools.h"
#include "BaseLocationManager.h"
#include "UnitInfoManager.h"
#include "WorkerManager.h"
#include "BotConfig.h"
#include "GameCommander.h"
#include "BuildingManager.h"
#include "StrategyManager.h"
#include "TechTree.h"
#include "BuildType.h"
#include "AutoObserver/CameraModule.h"
#include "Drawing.h"

class CCBot : public sc2::Agent
{
	std::map<int,sc2::Race> m_playerRace;

	MapTools                m_map;
	BaseLocationManager     m_bases;
	UnitInfoManager         m_unitInfo;
	WorkerManager           m_workers;
	StrategyManager         m_strategy;
	BotConfig               m_config;
	TechTree                m_techTree;

	GameCommander           m_gameCommander;
	CameraModuleAgent		m_cameraModule;

	int m_armor;
	int m_weapons;

	std::map<size_t, std::map<int,double>> m_time;

	void OnError(const std::vector<sc2::ClientError> & client_errors,
		const std::vector<std::string> & protocol_errors = {}) override;

	void OnUpgradeCompleted(sc2::UpgradeID upgrade) override;

public:

	CCBot();
	void OnGameStart() override;
	void OnStep() override;

	void OnBuildingConstructionComplete(const sc2::Unit * unit) override;
	void OnUnitCreated(const sc2::Unit * unit) override;
	void OnUnitEnterVision(const sc2::Unit * unit) override;

	void OnDTdetected(const sc2::Point2D pos);


	BotConfig & Config();
	WorkerManager & Workers();
	const BaseLocationManager & Bases() const;
	const MapTools & Map() const;
	UnitInfoManager & UnitInfo();

	const StrategyManager & Strategy() const;
	const TypeData & Data(const sc2::UnitTypeID & type) const;
	const TypeData & Data(const sc2::UpgradeID & type) const;
	const TypeData & Data(const BuildType & type) const;
	const sc2::Race & GetPlayerRace(int player) const;
	sc2::Point2D GetStartLocation() const;
	const CUnit_ptr GetUnit(const UnitTag & tag);
	const ProductionManager & Production();
	void requestGuards(const bool req);
	std::shared_ptr<shuttle> requestShuttleService(const CUnits passengers, const sc2::Point2D targetPos);

	const int getArmor() const;
	const int getWeapon() const;
	const bool underAttack() const;
};

extern bool useDebug;
extern bool useAutoObserver;

// We used to make a DLL for CommandCenter
// __declspec(dllexport) void *CreateNewAgent(); // Returns a pointer to a class deriving from sc2::Agent

// __declspec(dllexport) const char *GetAgentName(); // Returns a string identifier for the agent name

// __declspec(dllexport) int GetAgentRace(); // Returns the agents prefered race. should be sc2::Race cast to int.
