#pragma once

#include "Common.h"
#include "Squad.h"

class CCBot;

class SquadData
{
    CCBot & m_bot;
    std::map<std::string, Squad> m_squads;

    void    updateAllSquads();
    void    verifySquadUniqueMembership();

public:

    SquadData(CCBot & bot);

    void            onFrame();
    void            clearSquadData();

    bool            canAssignUnitToSquad(const sc2::Unit * unit, const Squad & squad) const;
    void            assignUnitToSquad(const sc2::Unit * unit, Squad & squad);
    void            addSquad(const std::string & squadName, const Squad & squad);
    void            removeSquad(const std::string & squadName);
    void            drawSquadInformation();


    bool            squadExists(const std::string & squadName);
    bool            unitIsInSquad(const sc2::Unit * unit) const;
    const Squad *   getUnitSquad(const sc2::Unit * unit) const;
    Squad *         getUnitSquad(const sc2::Unit * unit);

    Squad &         getSquad(const std::string & squadName);
    const std::map<std::string, Squad> & getSquads() const;

	const bool underAttack() const;
};
