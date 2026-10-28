#ifndef _EDGEBU2D_H
#define _EDGEBU2D_H

#include "EdgeWEDS.h"
#include "rg_BetaSpan.h"
#include "rg_Circle2D.h"

namespace V {
namespace GeometryTier {


class VertexBU2D;
class FaceBU2D;


class EdgeBU2D : public EdgeWEDS
{
private:
    rg_BetaSpan m_betaSpan;

public:
    EdgeBU2D();
    EdgeBU2D(const rg_INT& ID);
    EdgeBU2D(const EdgeBU2D& edge);
    ~EdgeBU2D();

    inline VertexBU2D* getStartVertex() const { return (VertexBU2D*) m_startVertex; }
    inline VertexBU2D* getEndVertex() const   { return (VertexBU2D*) m_endVertex; }
    inline FaceBU2D*   getLeftFace() const    { return (FaceBU2D*)   m_leftFace; }
    inline FaceBU2D*   getRightFace() const   { return (FaceBU2D*)   m_rightFace; }
    inline EdgeBU2D*   getLeftHand() const    { return (EdgeBU2D*)   m_leftHand; }
    inline EdgeBU2D*   getRightHand() const   { return (EdgeBU2D*)   m_rightHand; }
    inline EdgeBU2D*   getLeftLeg() const     { return (EdgeBU2D*)   m_leftLeg; }
    inline EdgeBU2D*   getRightLeg() const    { return (EdgeBU2D*)   m_rightLeg; }

    inline rg_REAL     getLowerBoundForValidSimplex() const { return m_betaSpan.getLowerValueOfInterval(1); }
	inline rg_INT      getBoundingState(const rg_REAL& beta) const { return m_betaSpan.findBoundingState(beta); }


	rg_BetaSpan getBetaSpan() const;

    rg_BOOL  isVirtual() const;
    rg_BOOL  isOnSqueezedHull() const;
    rg_BOOL  isAttached(const rg_Circle2D& minTangentCircle) const;


	void     computeBetaSpan();

    // Mokwon added 2017. 11. 29 for image processing
    void computeBetaSpan_horizontal_or_vertical_edge();
    void computeBetaSpan_diagonal_edge();
    ////////////////////////////////////////////////////

    EdgeBU2D& operator =(const EdgeBU2D& edge);

    ///////////////////////////////////////////////////////////////////////////
	//  Topological operators
    VertexBU2D* getVertexOfLeftHand() const;
    VertexBU2D* getVertexOfRightHand() const;
    VertexBU2D* getOppositeVertex( const VertexBU2D* const vertex ) const;

    rg_INT getNeighborVertices(rg_dList<VertexBU2D*>& vertexList) const;
    rg_INT getIncidentEdges(rg_dList<EdgeBU2D*>& edgeList) const;
    rg_INT getIncidentFaces(rg_dList<FaceBU2D*>& faceList) const;

    rg_INT getEdgesInStar(rg_dList<EdgeBU2D*>& edgeList) const;
    rg_INT getFacesInStar(rg_dList<FaceBU2D*>& faceList) const;

    ///////////////////////////////////////////////////////////////////////////
    //  Beta-operator
    rg_INT searchEdgesInBetaComplex( const rg_REAL& beta, rg_dList<EdgeBU2D*>& betaEdgeList ) const;
    rg_INT searchFacesInBetaComplex( const rg_REAL& beta, rg_dList<FaceBU2D*>& betaFaceList ) const;
    
    rg_INT searchEdgesInBetaComplex( const rg_REAL& beta, const rg_INT& boundingState, rg_dList<EdgeBU2D*>& betaEdgeList ) const;
    rg_INT searchFacesInBetaComplex( const rg_REAL& beta, const rg_INT& boundingState, rg_dList<FaceBU2D*>& betaFaceList ) const;

    rg_INT searchEdgesInBetaShape(   const rg_REAL& beta, rg_dList<EdgeBU2D*>& betaEdgeList ) const;

};


} // GeometryTier
} // V


#endif