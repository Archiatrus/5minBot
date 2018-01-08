#pragma once

#include "Common.h"
#include "BuildType.h"

class CCBot;

struct BuildOrderItem
{
    BuildType       type;		// the thing we want to 'build'
    int             priority;	// the priority at which to place it in the queue
    bool            blocking;	// whether or not we block further items

    BuildOrderItem(const BuildType & t, int p, bool b);
    bool operator<(const BuildOrderItem & x) const;
};
