#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"

class CCBot;

class CombatCommander
{
    CCBot &         m_bot;

    SquadData       m_squadData;
    std::vector<const sc2::Unit *>  m_combatUnits;
    bool            m_initialized;
    bool            m_attackStarted;

    void            updateScoutDefenseSquad();
    void            updateDefenseSquads();
    void            updateAttackSquads();
    void            updateIdleSquad();
    bool            isSquadUpdateFrame();

    const sc2::Unit * findClosestDefender(const Squad & defenseSquad, const sc2::Point2D & pos);
    const sc2::Unit * findClosestWorkerTo(std::vector<const sc2::Unit *> & unitsToAssign, const sc2::Point2D & target);


    sc2::Point2D    getMainAttackLocation();

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
    bool            shouldWeStartAttacking();

public:

    CombatCommander(CCBot & bot);


    void onStart();
    void onFrame(const std::vector<const sc2::Unit *> & combatUnits);

    void drawSquadInformation();
	const bool underAttack() const;
};

