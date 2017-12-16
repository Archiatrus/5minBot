#pragma once
#include "Util.h"
#include "CCBot.h"

class node
{
public:

	sc2::Point2D m_pos;
	int m_status; //-1 unvisited, 0 openlist, 1 closedList
	float m_travelCost;
	float m_heuristicToGoal;
	float m_threatLvl;
	float m_totalCost;
	std::shared_ptr<node> m_parent;

	node();

	node(sc2::Point2D pos, int status, float travelCost, float heuristicToGoal, float threatLvl, std::shared_ptr<node> parent=nullptr);

};

class pathPlaning
{
	CCBot &   m_bot;
	sc2::Point2D m_endPos;
	float m_stepSize;

	std::vector<std::vector<std::shared_ptr<node>>> m_map;
	std::list<std::shared_ptr<node>> m_openList;

	std::shared_ptr<node> map(sc2::Point2D pos) const;
	void setMap(sc2::Point2D pos, std::shared_ptr<node> node);
	void activateNode(sc2::Point2D pos, std::shared_ptr<node> parent);
	void updateNode(std::shared_ptr<node> updateNode, std::shared_ptr<node> parent);
	const float pathPlaning::calcHeuristic(sc2::Point2D pos) const;


	std::shared_ptr<node> getBestNextNodeAndPop();
	const bool isBiggerNode(std::shared_ptr<node> nodeA, std::shared_ptr<node> nodeB) const;
	const bool reachedEndPos(sc2::Point2D currentPos) const;
	
	void expandFrontNode(std::shared_ptr<node> frontNode);
	std::vector<sc2::Point2D> constructPath(std::shared_ptr<const node> frontNode) const;
	void insertInOpenList(const node * newNode);
	const float calcThreatLvl(sc2::Point2D pos) const;

public:
	pathPlaning(CCBot & bot, sc2::Point2D startPos, sc2::Point2D endPos, int mapWidth, int mapHeight,float stepSize);
	std::vector<sc2::Point2D> planPath();
};