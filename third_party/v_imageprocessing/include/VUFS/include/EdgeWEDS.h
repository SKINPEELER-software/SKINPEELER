#ifndef _EDGEWEDS_H
#define _EDGEWEDS_H

#include "rg_Const.h"
#include "TopologicalEntity.h"
#include "rg_dList.h"

class VertexWEDS;
class FaceWEDS;

class EdgeWEDS : public TopologicalEntity
{
protected:
    VertexWEDS* m_startVertex;
    VertexWEDS* m_endVertex;
    
    FaceWEDS*   m_leftFace;
    FaceWEDS*   m_rightFace;

    EdgeWEDS*   m_leftHand;
    EdgeWEDS*   m_rightHand;
    EdgeWEDS*   m_leftLeg;
    EdgeWEDS*   m_rightLeg;

    rg_BOOL     m_visited;


public:
    EdgeWEDS();
    EdgeWEDS(const rg_INT& ID);
    EdgeWEDS(const rg_INT& ID, VertexWEDS* startVertex, VertexWEDS* endVertex);
    EdgeWEDS(const rg_INT& ID, VertexWEDS* startVertex, VertexWEDS* endVertex,
                               FaceWEDS*   leftFace,    FaceWEDS*   rightFace,                              
                               EdgeWEDS*   leftHand,    EdgeWEDS*   rightHand,               
                               EdgeWEDS*   leftLeg,     EdgeWEDS*   rightLeg);
    EdgeWEDS(const EdgeWEDS& edge);
    ~EdgeWEDS();

    inline VertexWEDS* getStartVertex() const { return m_startVertex; }
    inline VertexWEDS* getEndVertex() const   { return m_endVertex; }
    inline FaceWEDS*   getLeftFace() const    { return m_leftFace; }
    inline FaceWEDS*   getRightFace() const   { return m_rightFace; }
    inline EdgeWEDS*   getLeftHand() const    { return m_leftHand; }
    inline EdgeWEDS*   getRightHand() const   { return m_rightHand; }
    inline EdgeWEDS*   getLeftLeg() const     { return m_leftLeg; }
    inline EdgeWEDS*   getRightLeg() const    { return m_rightLeg; }
    inline void        setStartVertex(  VertexWEDS* startVertex)  { m_startVertex = startVertex; }
    inline void        setEndVertex(    VertexWEDS* endVertex)    { m_endVertex = endVertex; }
    inline void        setLeftFace(     FaceWEDS*   leftFace)     { m_leftFace = leftFace; }
    inline void        setRightFace(    FaceWEDS*   rightFace)    { m_rightFace = rightFace; }
    inline void        setLeftHand(     EdgeWEDS*   leftHand)     { m_leftHand = leftHand; }
    inline void        setRightHand(    EdgeWEDS*   rightHand)    { m_rightHand = rightHand; }
    inline void        setLeftLeg(      EdgeWEDS*   leftLeg)      { m_leftLeg = leftLeg; }
    inline void        setRightLeg(     EdgeWEDS*   rightLeg)     { m_rightLeg = rightLeg; }

    void setVertices( VertexWEDS* startVertex, VertexWEDS* endVertex);
    void setLeft(  FaceWEDS* leftFace,  EdgeWEDS* leftHand,  EdgeWEDS* leftLeg);
    void setRight( FaceWEDS* rightFace, EdgeWEDS* rightHand, EdgeWEDS* rightLeg);

    void setEdge( VertexWEDS* startVertex, VertexWEDS* endVertex,
                  FaceWEDS*   leftFace,    FaceWEDS*   rightFace,                              
                  EdgeWEDS*   leftHand,    EdgeWEDS*   rightHand,               
                  EdgeWEDS*   leftLeg,     EdgeWEDS*   rightLeg);

    inline rg_BOOL  isVisited() const                  { return m_visited; };
    inline void     isVisited(const rg_BOOL& visited)  { m_visited = visited; };

    EdgeWEDS& operator=(const EdgeWEDS& edge);

	//  Topological operators
    inline rg_BOOL isStartVertex(VertexWEDS* vertex) const { return (vertex==m_startVertex)? rg_TRUE: rg_FALSE; }
    inline rg_BOOL isEndVertex(VertexWEDS* vertex) const   { return (vertex==m_endVertex)? rg_TRUE: rg_FALSE; }

    void connectRightHand( EdgeWEDS* rightHand);
    void connectLeftHand(  EdgeWEDS* leftHand);
    void connectRightLeg(  EdgeWEDS* rightLeg);
    void connectLeftLeg(   EdgeWEDS* leftLeg);

    void connectRightFace( FaceWEDS* rightFace);
    void connectLeftFace(  FaceWEDS* leftFace);

    void connectStartVertex( VertexWEDS* vertex );
    void connectEndVertex( VertexWEDS* vertex );

    void reverse();
    void flip();

    inline  rg_BOOL isIncidentTo(VertexWEDS* vertex) const {
                if ( m_startVertex == vertex || m_endVertex == vertex ) return rg_TRUE;
                else return rg_FALSE; 
            }
    rg_BOOL     isConnected(EdgeWEDS* otherEdge) const;

    VertexWEDS* getVertexOfLeftHand() const;
    VertexWEDS* getVertexOfRightHand() const;

	//  Topological operators
    rg_INT getNeighborVertices(rg_dList<VertexWEDS*>& vertexList) const;

    rg_INT getIncidentEdges(rg_dList<EdgeWEDS*>& edgeList) const;
    rg_INT getIncidentFaces(rg_dList<FaceWEDS*>& faceList) const;

    rg_INT getEdgesInStar(rg_dList<EdgeWEDS*>& edgeList) const;

    rg_INT getFacesInStar(rg_dList<FaceWEDS*>& faceList) const;

};


#endif

