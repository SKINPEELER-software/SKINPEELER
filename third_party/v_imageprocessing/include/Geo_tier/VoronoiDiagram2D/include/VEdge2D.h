#ifndef VEDGE2D_H
#define VEDGE2D_H

#include "rg_RQBzCurve2D.h"

#include <list>
using namespace std;

namespace V {
namespace GeometryTier {

class VVertex2D;
class VFace2D;
class Generator2D;

enum STATUS_OF_VEDGE
{
    RED_E,        // edge in the red ocean
    PURPLE_E,     // intersecting edge
    WHITE_E       // default color
};

class VEdge2D
{
private:
    int                 m_ID;

    VVertex2D*          m_startVertex;
    VVertex2D*          m_endVertex;

    VFace2D*            m_leftFace;
    VFace2D*            m_rightFace;

    VEdge2D*            m_leftHand;
    VEdge2D*            m_rightHand;
    VEdge2D*            m_leftLeg;
    VEdge2D*            m_rightLeg;

    STATUS_OF_VEDGE     m_status;
    rg_RQBzCurve2D      m_geometry;

    bool                m_isAnomalyTestDone;
    bool                m_isCandidateToBeFlippedInPhantomRemoval;

	rg_Circle2D         m_tangentCircle;    // tangent circle of two generators for disk packing by Joonghyun March, 15

    double              m_minClearance;
    double              m_maxClearance;

    bool                m_isThereContactBetweenTwoGenerators;
    bool                m_isVisited;

    bool                m_radiusIntervalIsCalculated;
    double              m_radiusIntervalOfTangentCircles[2];


    void*               m_userData; // August 10, 2020 by CYSONG.

public:
    VEdge2D();
    VEdge2D( const int& ID );
    VEdge2D( const int& ID, VVertex2D* const startVertex, VVertex2D* const endVertex );
    VEdge2D( const VEdge2D& edge );
    ~VEdge2D();

    inline int              getID() const { return m_ID; }
    inline VVertex2D*       getStartVertex() const { return m_startVertex; }
    inline VVertex2D*       getEndVertex() const { return m_endVertex; }
    inline VFace2D*         getLeftFace() const { return m_leftFace; }
    inline VFace2D*         getRightFace() const { return m_rightFace; }
    inline VEdge2D*         getLeftHand() const { return m_leftHand; }
    inline VEdge2D*         getRightHand() const { return m_rightHand; }
    inline VEdge2D*         getLeftLeg() const { return m_leftLeg; }
    inline VEdge2D*         getRightLeg() const { return m_rightLeg; }
    inline STATUS_OF_VEDGE  getStatus() const { return m_status; }
    inline rg_RQBzCurve2D   getGeometry() const { return m_geometry; }
    inline double           getMinClearance() const { return m_minClearance; }
    inline double           getMaxClearance() const { return m_maxClearance; }
	inline rg_Circle2D      getTangentCircle() const { return m_tangentCircle; } // for disk packing by Joonghyun March, 15
	
    inline void*            getUserData()  const { return m_userData; }

                                                                                 //JKKIM_ADDED
    VFace2D*                getMateFace(VVertex2D* vertex) const;
    inline bool             isThereContactBetweenTwoGenerators() const { return m_isThereContactBetweenTwoGenerators; }
    inline void             isThereContactBetweenTwoGenerators( const bool& true_false ) { m_isThereContactBetweenTwoGenerators = true_false; }
	
    inline void             setID( const int& ID ) { m_ID = ID; }
    inline void             setStartVertex( VVertex2D* const startVertex ) { m_startVertex = startVertex; }
    inline void             setEndVertex( VVertex2D* const endVertex ) { m_endVertex = endVertex; }
    inline void             setLeftFace( VFace2D* const leftFace ) { m_leftFace = leftFace; }
    inline void             setRightFace( VFace2D* const rightFace ) { m_rightFace = rightFace; }
    inline void             setLeftHand( VEdge2D* const leftHand ) { m_leftHand = leftHand; }
    inline void             setRightHand( VEdge2D* const rightHand ) { m_rightHand = rightHand; }
    inline void             setLeftLeg( VEdge2D* const leftLeg ) { m_leftLeg = leftLeg; }
    inline void             setRightLeg( VEdge2D* const rightLeg ) { m_rightLeg = rightLeg; }
    inline void             setStatus( const STATUS_OF_VEDGE& status ) { m_status = status; }
    inline void             setMinClearance( const double& clearance ) { m_minClearance = clearance; }
    inline void             setMaxClearance( const double& clearance ) { m_maxClearance = clearance; }
	inline void             setTangentCircle(const rg_Circle2D& tangentCircle) { m_tangentCircle = tangentCircle; } // for disk packing by Joonghyun March, 15
    void                    setGeometry(const rg_Point2D& sp, 
                                        const rg_Point2D& tvs, 
                                        const rg_Point2D& ep, 
                                        const rg_Point2D& tve, 
                                        const rg_Point2D& passPt);
    void                    setGeometry(const rg_RQBzCurve2D& geometry);
    void                    setGeometryLine();

    VEdge2D&                operator=( const VEdge2D& edge );
    bool                    operator==( const VEdge2D& edge ) const;
    
    void                    getIncidentVVertices( list<VVertex2D*>& incidentVVerticesList ) const;
    void                    getIncidentVFaces( list<VFace2D*>& incidentVFacesList ) const;
    void                    getAdjacentVEdges( list<VEdge2D*>& adjacentVEdgesList ) const;
    VVertex2D*              getOppositVVertex( VVertex2D* vertex ) const;

    void                    setTopology( VVertex2D* const startVertex, VVertex2D* const endVertex,
                                         VFace2D* const leftFace, VFace2D* const rightFace,
                                         VEdge2D* const leftHand, VEdge2D* const rightHand,
                                         VEdge2D* const leftLeg, VEdge2D* const rightLeg );
    void                    setTopology( VFace2D* const leftFace, VFace2D* const rightFace,
                                         VEdge2D* const leftHand, VEdge2D* const rightHand,
                                         VEdge2D* const leftLeg, VEdge2D* const rightLeg );
    void                    setTopology( VEdge2D* const leftHand, VEdge2D* const rightHand,
                                         VEdge2D* const leftLeg, VEdge2D* const rightLeg );

    inline void             setUserData(void* userData) { m_userData = userData; }


    bool    isInfinite() const;
    bool    isBounded() const;
    bool    isUnBounded() const;

    // MOKWON ADDED FOR VDCIC::SEED_VD.
    bool    isEllipticEdge() const;

    bool    isMemberOf( const VFace2D* const face ) const;
    bool    isAdjacentTo( const VEdge2D* const edge ) const;
    bool    isIncidentTo( const VVertex2D* const vertex ) const;

    void    flip();
    void    flip_reverse();


    inline bool     isAnomalyTestDone() const { return m_isAnomalyTestDone; }
    inline void     setTrueAnomalyTestDone()  { m_isAnomalyTestDone = true; }
    inline void     setFalseAnomalyTestDone() { m_isAnomalyTestDone = false; }

    inline bool     isCandidateToBeFlippedInPhantomRemoval() const { return m_isCandidateToBeFlippedInPhantomRemoval; }
    inline void     isCandidateToBeFlippedInPhantomRemoval( const bool& isCandidateOrNot ) { m_isCandidateToBeFlippedInPhantomRemoval = isCandidateOrNot; }

    inline bool     isVisited() const { return m_isVisited; }
    inline void     isVisited( const bool& isVisitedOrNot ) { m_isVisited = isVisitedOrNot; }
    //void    compute_min_max_clearance( double& minClearance, double& maxClearance );

    //YOUNGSONG_ADDED FOR SHORTEST PATH ALGO.
    double          minimum_radius_of_tangent_circles() const;
    double          maximum_radius_of_tangent_circles() const;
    bool            is_passable(const double& probeRadius) const;
    void            compute_radius_interval_of_tangent_circles();

    inline void     set_radius_interval_is_calculated (const bool& intervalCalculated) { m_radiusIntervalIsCalculated = intervalCalculated; };
    inline bool     radius_interval_is_calculated() { return m_radiusIntervalIsCalculated; };
};


inline bool VEdge2D::isEllipticEdge() const { return ( m_startVertex == m_endVertex ); }

inline  double  VEdge2D::minimum_radius_of_tangent_circles() const { return m_radiusIntervalOfTangentCircles[0]; }
inline  double  VEdge2D::maximum_radius_of_tangent_circles() const { return m_radiusIntervalOfTangentCircles[1]; }
inline  bool    VEdge2D::is_passable(const double& probeRadius) const { return (probeRadius < m_radiusIntervalOfTangentCircles[0]); }



} // GeometryTier
} // V

#endif


