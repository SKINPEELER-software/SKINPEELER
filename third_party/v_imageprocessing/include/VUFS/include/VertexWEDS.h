#ifndef _VERTEXWEDS_H
#define _VERTEXWEDS_H

#include "rg_Const.h"
#include "TopologicalEntity.h"
#include "rg_dList.h"

class EdgeWEDS;
class FaceWEDS;


class VertexWEDS : public TopologicalEntity
{
protected:
    EdgeWEDS* m_firstEdge;

    rg_BOOL   m_visited;


public:
    VertexWEDS();
    VertexWEDS(const rg_INT& ID);
    VertexWEDS(const rg_INT& ID, EdgeWEDS* firstEdge);
    VertexWEDS(const VertexWEDS& vertex);
    ~VertexWEDS();

    inline EdgeWEDS* getFirstEdge()  const             { return m_firstEdge; };
    inline void      setFirstEdge(EdgeWEDS* firstEdge) { m_firstEdge = firstEdge; };

    inline rg_BOOL  isVisited() const                  { return m_visited; };
    inline void     isVisited(const rg_BOOL& visited)  { m_visited = visited; };

    VertexWEDS& operator=(const VertexWEDS& vertex);
    
    EdgeWEDS* getCCWNextEdge(const EdgeWEDS* const currEdge) const;
    EdgeWEDS* getCWNextEdge(const EdgeWEDS* const currEdge) const;

	//  Topological operators
	rg_INT    getNeighborVertices(  rg_dList<VertexWEDS*>& vertexList) const;
	rg_INT    getIncidentEdges(     rg_dList<EdgeWEDS*>& edgeList) const;
	rg_INT    getIncidentFaces(     rg_dList<FaceWEDS*>& faceList) const;

    rg_INT    getEdgesInStar(       rg_dList<EdgeWEDS*>& edgeList) const;
    rg_INT    getEdgesInShell(      rg_dList<EdgeWEDS*>& edgeList) const;

    EdgeWEDS* findConnectingEdge(   VertexWEDS* vertex) const;
    rg_INT    findSharingFace(      VertexWEDS* vertex, rg_dList<FaceWEDS*>& faceList) const;

};


#endif

