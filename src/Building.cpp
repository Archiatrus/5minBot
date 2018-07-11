#include "Building.h"

Building::Building() 
	: desiredPosition   (0,0)
	, finalPosition	 (0,0)
	, position		  (0,0)
	, m_type			  (0)
	, buildingUnit	  (nullptr)
	, builderUnit	   (nullptr)
	, lastOrderFrame	(0)
	, status			(BuildingStatus::Unassigned)
	, buildCommandGiven (false)
	, underConstruction (false) 
{} 

// constructor we use most often
Building::Building(sc2::UnitTypeID t, sc2::Point2D desired, int width)
	: finalPosition	 (0,0)
	, position		  (0,0)
	, m_type			  (t)
	, buildingUnit	  (nullptr)
	, builderUnit	   (nullptr)
	, lastOrderFrame	(0)
	, status			(BuildingStatus::Unassigned)
	, buildCommandGiven (false)
	, underConstruction (false) 
{
	
	if (width % 2 == 1)
	{
		desiredPosition = sc2::Point2D(std::floor(desired.x) + 0.5f, std::floor(desired.y) + 0.5f);
	}
	else
	{
		desiredPosition = sc2::Point2D(std::floor(desired.x), std::floor(desired.y));
	}
}

// equals operator
bool Building::operator == (const Building & b) 
{
	// buildings are equal if their worker unit and building unit are equal
	return  ((!b.buildingUnit && !buildingUnit) || (b.buildingUnit && buildingUnit && b.buildingUnit->getTag() == buildingUnit->getTag()))
			&& ((!b.builderUnit && !builderUnit) || (b.builderUnit && builderUnit && b.builderUnit->getTag() == builderUnit->getTag()))
			&& b.m_type==m_type
			&& (b.finalPosition.x == finalPosition.x)
			&& (b.finalPosition.y == finalPosition.y);
}