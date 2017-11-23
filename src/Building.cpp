#include "Building.h"

Building::Building() 
    : desiredPosition   (0,0)
    , finalPosition     (0,0)
    , position          (0,0)
    , type              (0)
    , buildingUnit      (nullptr)
    , builderUnit       (nullptr)
    , lastOrderFrame    (0)
    , status            (BuildingStatus::Unassigned)
    , buildCommandGiven (false)
    , underConstruction (false) 
{} 

// constructor we use most often
Building::Building(sc2::UnitTypeID t, sc2::Point2D desired)
    : desiredPosition   (desired)
    , finalPosition     (0,0)
    , position          (0,0)
    , type              (t)
    , buildingUnit      (nullptr)
    , builderUnit       (nullptr)
    , lastOrderFrame    (0)
    , status            (BuildingStatus::Unassigned)
    , buildCommandGiven (false)
    , underConstruction (false) 
{}

// equals operator
bool Building::operator == (const Building & b) 
{
	// buildings are equal if their worker unit and building unit are equal
    return      (b.buildingUnit == buildingUnit) 
             && (b.builderUnit  == builderUnit) 
             && (b.finalPosition.x == finalPosition.x)
             && (b.finalPosition.y == finalPosition.y);
}