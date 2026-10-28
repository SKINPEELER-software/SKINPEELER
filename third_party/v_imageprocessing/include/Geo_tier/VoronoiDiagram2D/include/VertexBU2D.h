#ifndef _VERTEXBU2D_H
#define _VERTEXBU2D_H

#include "VertexWEDS.h"
#include "rg_BetaSpan.h"
#include "Disc.h"

namespace V {
namespace GeometryTier {

class EdgeBU2D;
class FaceBU2D;

class VertexBU2D : public VertexWEDS
{
private:
    rg_BetaSpan m_betaSpan;
    Disc*    m_disc;

public:
    VertexBU2D();
    VertexBU2D(const rg_INT& ID, Disc* disc);
    VertexBU2D(const VertexBU2D& vertex);
    ~VertexBU2D();

    Disc*      getDisc();
    rg_Point2D  getCoord() const; 
    rg_Circle2D getCircle() const;

	rg_BetaSpan getBetaSpan() const;

    inline EdgeBU2D* getFirstEdge()  const { return (EdgeBU2D*) m_firstEdge; }

    inline rg_BOOL isVirtual() const { return ( m_disc == rg_NULL ) ? rg_TRUE : rg_FALSE; }
    inline rg_REAL getLowerBoundForValidSimplex() const { return m_betaSpan.getLowerValueOfInterval(1); }
	inline rg_INT  getBoundingState(const rg_REAL& beta) const { return m_betaSpan.findBoundingState(beta); }

    void     setDisc(Disc* disc);

    rg_BOOL  isOnSqueezedHull() const;

    void     computeBetaSpan();

    VertexBU2D& operator =(const VertexBU2D& vertex);

    ///////////////////////////////////////////////////////////////////////////
	//  Topological operators
	rg_INT    getNeighborVertices(  rg_dList<VertexBU2D*>& vertexList) const;
	rg_INT    getIncidentEdges(     rg_dList<EdgeBU2D*>& edgeList) const;
	rg_INT    getIncidentFaces(     rg_dList<FaceBU2D*>& faceList) const;

    rg_INT    getEdgesInStar(       rg_dList<EdgeBU2D*>& edgeList) const;
    rg_INT    getEdgesInShell(      rg_dList<EdgeBU2D*>& edgeList) const;

    EdgeBU2D* findConnectingEdge(   VertexBU2D* vertex) const;
    rg_INT    findSharingFace(      VertexBU2D* vertex, rg_dList<FaceBU2D*>& faceList) const;


    ///////////////////////////////////////////////////////////////////////////
    //  Beta-operator
    rg_INT searchVerticesInBetaComplex(const rg_REAL& beta, rg_dList<VertexBU2D*>& betaVertexList ) const;
    rg_INT searchEdgesInBetaComplex(   const rg_REAL& beta, rg_dList<EdgeBU2D*>&   betaEdgeList   ) const;
    rg_INT searchFacesInBetaComplex(   const rg_REAL& beta, rg_dList<FaceBU2D*>&   betaFaceList   ) const;

    rg_INT searchVerticesInBetaComplex(const rg_REAL& beta, const rg_INT& boundingState, rg_dList<VertexBU2D*>& betaVertexList ) const;
    rg_INT searchEdgesInBetaComplex(   const rg_REAL& beta, const rg_INT& boundingState, rg_dList<EdgeBU2D*>& betaEdgeList ) const;
    rg_INT searchFacesInBetaComplex(   const rg_REAL& beta, const rg_INT& boundingState, rg_dList<FaceBU2D*>& betaFaceList ) const;

    rg_INT searchVerticesInBetaShape( const rg_REAL& beta, rg_dList<VertexBU2D*>& betaVertexList ) const;
    rg_INT searchEdgesInBetaShape(    const rg_REAL& beta, rg_dList<EdgeBU2D*>&   betaEdgeList   ) const;

};


} // GeometryTier
} // V


#endif

