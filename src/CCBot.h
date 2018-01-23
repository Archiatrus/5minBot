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
#include "AutoObserver\CameraModule.h"
#include "Drawing.h"

class CCBot : public sc2::Agent 
{
    sc2::Race               m_playerRace[2];

    MapTools                m_map;
    BaseLocationManager     m_bases;
    UnitInfoManager         m_unitInfo;
    WorkerManager           m_workers;
    StrategyManager         m_strategy;
    BotConfig               m_config;
    TechTree                m_techTree;

    GameCommander           m_gameCommander;
	CameraModuleAgent		m_cameraModule;

    void OnError(const std::vector<sc2::ClientError> & client_errors, 
                 const std::vector<std::string> & protocol_errors = {}) override;

public:

    CCBot();
    void OnGameStart() override;
    void OnStep() override;

	void OnUnitCreated(const sc2::Unit * unit) override;
	void OnBuildingConstructionComplete(const sc2::Unit * unit) override;
	void OnUnitEnterVision(const sc2::Unit * unit) override;

	void OnDTdetected(const sc2::Point2D pos);

          BotConfig & Config();
          WorkerManager & Workers();
    const BaseLocationManager & Bases() const;
    const MapTools & Map() const;
    const UnitInfoManager & UnitInfo() const;
    const StrategyManager & Strategy() const;
    const TypeData & Data(const sc2::UnitTypeID & type) const;
    const TypeData & Data(const sc2::UpgradeID & type) const;
    const TypeData & Data(const BuildType & type) const;
    const sc2::Race & GetPlayerRace(int player) const;
    sc2::Point2D GetStartLocation() const;
    const sc2::Unit * GetUnit(const UnitTag & tag) const;
	const ProductionManager & Production();
};

extern bool useDebug;
extern bool useAutoObserver;

__declspec(dllexport) void *CreateNewAgent(); // Returns a pointer to a class deriving from sc2::Agent

__declspec(dllexport) const char *GetAgentName(); // Returns a string identifier for the agent name

__declspec(dllexport) int GetAgentRace(); // Returns the agents prefered race. should be sc2::Race cast to int.