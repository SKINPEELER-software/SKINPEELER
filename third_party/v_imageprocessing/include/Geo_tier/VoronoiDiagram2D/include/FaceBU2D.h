#ifndef _FACEBU2D_H
#define _FACEBU2D_H

#include "FaceWEDS.h"
#include "rg_BetaSpan.h"
#include "rg_Circle2D.h"


namespace V {
namespace GeometryTier {

class VertexBU2D;
class EdgeBU2D;

class FaceBU2D : public FaceWEDS
{
private:
    rg_BetaSpan    m_betaSpan;

    rg_Circle2D m_emptyTangentCircle;

public:
    FaceBU2D();
    FaceBU2D(const rg_INT& ID);
    FaceBU2D(const rg_INT& ID, const rg_Circle2D& emptyTangentCircle);
    FaceBU2D(const FaceBU2D& face);
    ~FaceBU2D();


	rg_BetaSpan    getBetaSpan() const;
    rg_Circle2D getEmptyTangentCircle() const;

    inline EdgeBU2D* getFirstEdge()  const { return (EdgeBU2D*) m_firstEdge; }
    inline rg_REAL   getLowerBoundForValidSimplex() const { return m_betaSpan.getLowerValueOfInterval(1); }
	inline rg_INT    getBoundingState(const rg_REAL& beta) const { return m_betaSpan.findBoundingState(beta); }

    rg_BOOL     isVirtual() const;

    void        setEmptyTangentCircle(const rg_Circle2D& emptyTangentCircle);


	void        computeBetaSpan();

    FaceBU2D& operator =(const FaceBU2D& face);

    rg_REAL     computeSignedArea() const;


	///////////////////////////////////////////////////////////////////////////
    //  Topological operators
    rg_INT getBoundingVertices(rg_dList<VertexBU2D*>& boundingVertices) const;
    rg_INT getBoundingEdges(   rg_dList<EdgeBU2D*>&   boundingEdges) const;
    rg_INT getAdjacentFaces(   rg_dList<FaceBU2D*>&   faceList) const;


    ///////////////////////////////////////////////////////////////////////////
    //  Beta-operator
    FaceBU2D* getOppositeFaceWithAnomalyInQuasiTriangulation() const;
    FaceBU2D* getOppositeFaceWithAnomalyInBetaComplex(const rg_REAL& betaValue) const;

    rg_BOOL   isAnomalyInQuasiTriangulation() const;
    rg_BOOL   isAnomalyInBetaComplex(const rg_REAL& betaValue) const;

    rg_INT searchFacesInBetaComplex( const rg_REAL& beta, rg_dList<FaceBU2D*>& betaFaceList ) const;
    rg_INT searchFacesInBetaComplex( const rg_REAL& beta, const rg_INT& boundingState, rg_dList<FaceBU2D*>& betaFaceList ) const;

};


} // GeometryTier
} // V


#endif