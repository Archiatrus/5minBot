#pragma once

#include "Common.h"
#include "sc2api/sc2_api.h"
#include "CUnit.h"

class CCBot;

namespace Micro
{   
    void SmartStop          (CUnit_ptr attacker,  CCBot & bot);
    void SmartAttackUnit    (CUnit_ptr attacker, CUnit_ptr target, CCBot & bot, bool queue=false);
	void SmartAttackUnit	(const CUnits & attacker, const  CUnit_ptr target, CCBot & bot, bool queue=false);
    void SmartAttackMove    (CUnit_ptr attacker,  const sc2::Point2D & targetPosition, CCBot & bot);
	void SmartAttackMove    (CUnits & attacker, const sc2::Point2D & targetPosition, CCBot & bot);
	void SmartMove(const CUnits attackers, const CUnit_ptr target, CCBot & bot, const bool queue = false);
	void SmartAttackMoveToUnit(CUnits & attacker, const CUnit_ptr target, CCBot & bot);
	void SmartMove(CUnit_ptr attacker, CUnit_ptr target, CCBot & bot, bool queue = false);
    void SmartMove          (CUnit_ptr attacker,  const sc2::Point2D & targetPosition, CCBot & bot, bool queue = false);
	void SmartMove(CUnits attackers, const sc2::Point2D & targetPosition, CCBot & bot, bool queue=false);
    void SmartRightClick    (CUnit_ptr unit,      CUnit_ptr target, CCBot & bot, bool queue = false);
	void SmartRightClick	(CUnits units, CUnit_ptr target, CCBot & bot, bool queue=false);
	void SmartRightClick	(CUnit_ptr unit, CUnits targets, CCBot & bot);
    void SmartRepair        (CUnit_ptr unit,      CUnit_ptr target, CCBot & bot);
    void SmartKiteTarget    (CUnit_ptr rangedUnit,CUnit_ptr target, CCBot & bot,bool queue = false);
    void SmartBuild         (CUnit_ptr builder,   const sc2::UnitTypeID & buildingType, sc2::Point2D pos, CCBot & bot);
    void SmartBuildTarget   (CUnit_ptr builder,   const sc2::UnitTypeID & buildingType, CUnit_ptr target, CCBot & bot);
    void SmartTrain         (CUnit_ptr builder,   const sc2::UnitTypeID & buildingType, CCBot & bot);
    void SmartAbility       (CUnit_ptr builder,   const sc2::AbilityID & abilityID, CCBot & bot);
	void SmartAbility(CUnits units, const sc2::AbilityID & abilityID, CCBot & bot, bool queue=false);
	void SmartAbility		(CUnit_ptr unit, const sc2::AbilityID & abilityID, const sc2::Point2D pos, CCBot & bot, bool queue=false);
	void SmartAbility(CUnit_ptr unit, const sc2::AbilityID & abilityID, CUnit_ptr target, CCBot & bot, bool queue=false);
	void SmartCDAbility		(CUnit_ptr builder, const sc2::AbilityID & abilityID, CCBot & bot,bool queue=false);
	void SmartCDAbility(CUnits units, const sc2::AbilityID & abilityID, CCBot & bot, bool queue=false);
	void SmartStim(CUnits units, CCBot & bot, bool queue=false);
};