#pragma once

#include "CUnit.h"
#include "DistanceMap.h"

class CCBot;

class BaseLocation
{
	CCBot &					 m_bot;
	DistanceMap				 m_distanceMap;

	sc2::Point2D				m_centerOfResources;
	sc2::Point2D				m_centerOfMinerals;
	sc2::Point2D				m_centerOfBase;
	CUnit_ptr					m_townhall;
	CUnits						m_geysers;
	CUnits						m_minerals;

	std::vector<sc2::Point2D>	m_mineralPositions;
	std::vector<sc2::Point2D>	m_geyserPositions;

	std::map<int, bool>		m_isPlayerOccupying;
	std::map<int, bool>		m_isPlayerStartLocation;
		
	int						m_baseID;
	float					m_left;
	float					m_right;
	float					m_top;
	float					m_bottom;
	bool					m_isStartLocation;
	int						m_numEnemyCombatUnits;
public:

	BaseLocation(CCBot & bot, int baseID, const CUnits resources);
	
	int getGroundDistance(const sc2::Point2D & pos) const;
	bool isStartLocation() const noexcept;
	bool isPlayerStartLocation(int player) const;
	bool isMineralOnly() const;
	void resetNumEnemyCombatUnits() noexcept;
	void incrementNumEnemyCombatUnits();
    int getNumEnemyCombatUnits() const;
    int getNumEnemyStaticD() const;
    int getBaseID() const;
	bool containsPosition(const sc2::Point2D & pos) const;
	//const sc2::Point2D & getDepotPosition() const;
	const sc2::Point2D & getCenterOfRessources() const noexcept;
	const sc2::Point2D & getCenterOfMinerals() const noexcept;
	const sc2::Point2D & getCenterOfBase() const noexcept;
	const CUnits & getGeysers() const noexcept;
	const CUnits & getMinerals() const noexcept;
	bool isOccupiedByPlayer(int player) const;
	bool isExplored() const;
	bool isInResourceBox(int x, int y) const noexcept;

	void setPlayerOccupying(int player, bool occupying);
	void setTownHall(const CUnit_ptr townHall);

	const std::vector<sc2::Point2D> & getClosestTiles() const;

	const CUnit_ptr getTownHall() const noexcept;

	void draw();
};
