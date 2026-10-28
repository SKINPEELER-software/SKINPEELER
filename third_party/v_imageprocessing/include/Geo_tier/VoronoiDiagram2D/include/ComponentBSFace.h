#ifndef _COMPONENT_BS_FACE_H
#define _COMPONENT_BS_FACE_H

//#include "BetaFaceForClassification.h"
#include "FaceBU2D.h"
#include "EdgeBU2D.h"
#include "VertexBU2D.h"
#include "LoopOfComponent.h"
#include "rg_Point2D.h"
#include "VEdge2D.h"
#include "NeighborInfo.h"
#include <map>
//#include <unordered_map>

namespace V {
namespace GeometryTier {

class ComponentBSFace
{
public:
    map<ComponentBSFace*, int>          m_neighborInfo;
    rg_dList<VEdge2D*>                  m_boundaryVEdges;
    rg_dList<NeighborInfo>              m_neighbors;
    bool                                m_isConsiderable;

private:
    double                              m_betaValue;
    int                                 m_ID;
    rg_dList<FaceBU2D*>                 m_betaFaces;
    rg_dList<LoopOfComponent>           m_loops;
    LoopOfComponent*                    m_outerLoop;
    rg_dList<LoopOfComponent*>          m_innerLoops;
    map<EdgeBU2D*, LoopOfComponent*>    m_mapEdgeToLoop;
    // Added by Joonghyun on November 18, 2020
    //unordered_map<EdgeBU2D*, LoopOfComponent*>    m_mapEdgeToLoop;

    double                              m_circularity; // 동일한 면적을 갖는 원의 둘레 / component의 제일 긴 loop의 길이
    double                              m_area;
    rg_Point2D                          m_centroid;
    double                              m_perimeter;

public:
    ComponentBSFace(void);
    ComponentBSFace(const int& ID);
    ComponentBSFace(const ComponentBSFace& component);
    ~ComponentBSFace(void);

    rg_dList<FaceBU2D*>&                getBetaFaces();
    rg_dList<LoopOfComponent>&          getLoops();
    int                                 getID() const;
    LoopOfComponent*                    getOuterLoop();
    rg_dList<LoopOfComponent*>&         getInnerLoops();
    double                              getArea() const;
    rg_Point2D                          getCentroid() const;
    double                              getCircularity() const;
    double                              getPerimeter() const;
    double                              getBetaValue() const;

    void                                setBetaValue( const double& betaValue );

    void                                assignOuterLoop( LoopOfComponent* loop );
    LoopOfComponent*                    setOuterLoopAsLongestLoop();
    void                                insertEdgeLoopPair(EdgeBU2D* edge, LoopOfComponent* loop);
    LoopOfComponent*                    findLoopIncludingThisEdge(EdgeBU2D* edge);
    bool                                isThereNeighborList(const int& ID, NeighborInfo*& neighbor);

    void                                addBetaFace(FaceBU2D* betaFace);

    void                                computeArea();
    void                                computePerimeter( const double& betaValue );
    void                                computeCentroid();
    void                                computeCircularity_with_area_N_Perimeter();
    void                                computeCircularity(const double& betaValue);
    void                                computeLoops();

private:
    void                                classifyLoopsAsAnOuter_N_innerLoops();
    void                                propagateLoopWithOrientation( const EdgeBU2D* const seedEdge, LoopOfComponent* const loop );
    //void                                fillUp(const double& betaValue);
};


inline rg_dList<FaceBU2D*>&         ComponentBSFace::getBetaFaces() { return m_betaFaces; }
inline rg_dList<LoopOfComponent>&   ComponentBSFace::getLoops() { return m_loops; }
inline int                          ComponentBSFace::getID() const { return m_ID; }
inline LoopOfComponent*             ComponentBSFace::getOuterLoop() { return m_outerLoop; }
inline rg_dList<LoopOfComponent*>&  ComponentBSFace::getInnerLoops() { return m_innerLoops; }
inline double                       ComponentBSFace::getArea() const { return m_area; }
inline rg_Point2D                   ComponentBSFace::getCentroid() const { return m_centroid; }
inline double                       ComponentBSFace::getCircularity() const { return m_circularity; }
inline double                       ComponentBSFace::getPerimeter() const { return m_perimeter; }
inline double                       ComponentBSFace::getBetaValue() const { return m_betaValue; }
inline void                         ComponentBSFace::setBetaValue( const double& betaValue ) { m_betaValue = betaValue; }
inline void                         ComponentBSFace::assignOuterLoop( LoopOfComponent* loop ) { m_outerLoop = loop; }


} // GeometryTier
} // V
#endif

