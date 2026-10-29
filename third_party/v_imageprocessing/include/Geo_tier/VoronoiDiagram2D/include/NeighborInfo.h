#ifndef NEIGHBOR_INFO_H
#define NEIGHBOR_INFO_H

#include "rg_Point2D.h"
#include "VEdge2D.h"
#include "rg_dList.h"


namespace V {
namespace GeometryTier {


class NeighborInfo
{
public:
    int                 m_ID;
    double              m_totalDist;
    double              m_minDist;
    double              m_maxDist;
    rg_Point2D          minPT_currComp;
    rg_Point2D          minPT_neighborComp;
    rg_Point2D          maxPT_currComp;
    rg_Point2D          maxPT_neighborComp;
    rg_dList<VEdge2D*>  edgesBtwNeighbor;


public:
    NeighborInfo(void);
    ~NeighborInfo(void);
};


} // GeometryTier
} // V


#endif


