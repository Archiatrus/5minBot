#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class MeleeManager: public MicroManager
{

public:

    MeleeManager(CCBot & bot);
    void    executeMicro(const CUnits & targets);
    void    assignTargets(const CUnits & targets);
    int     getAttackPriority(const CUnit_ptr attacker, const CUnit_ptr unit);
	const CUnit_ptr getTarget(const CUnit_ptr meleeUnit, const CUnits & targets);
    bool    meleeUnitShouldRetreat(const CUnit_ptr meleeUnit,const CUnits & targets);
};
