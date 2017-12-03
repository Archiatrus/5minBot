#pragma once

#include "sc2api/sc2_api.h"
#include "BaseLocation.h"

class CCBot;

class BaseLocationManager
{
    CCBot & m_bot;

    std::vector<BaseLocation>                       m_baseLocationData;
    std::vector<const BaseLocation *>               m_baseLocationPtrs;
    std::vector<const BaseLocation *>               m_startingBaseLocations;
    std::map<int, const BaseLocation *>             m_playerStartingBaseLocations;
    std::map<int, std::set<const BaseLocation *>>   m_occupiedBaseLocations;
    std::vector<std::vector<BaseLocation *>>        m_tileBaseLocations;

public:

    BaseLocationManager(CCBot & bot);
    
    void onStart();
    void onFrame();
    void drawBaseLocations();

    const std::vector<const BaseLocation *> & getBaseLocations() const;
	BaseLocation * getBaseLocation(const sc2::Point2D & pos) const;
    const std::vector<const BaseLocation *> & getStartingBaseLocations() const;
    const std::set<const BaseLocation *> & getOccupiedBaseLocations(int player) const;
	const sc2::Point2D getBuildingLocation() const;
    const BaseLocation * getPlayerStartingBaseLocation(int player) const;
    
    sc2::Point2D getNextExpansion(int player) const;
	sc2::Point2D getNewestExpansion(int player) const;
	const sc2::Point2D getRallyPoint() const;
	void BaseLocationManager::assignTownhallToBase(const sc2::Unit * townHall) const;
};
