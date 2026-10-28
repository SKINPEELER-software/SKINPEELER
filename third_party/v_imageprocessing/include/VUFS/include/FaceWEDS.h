#ifndef _FACEWEDS_H
#define _FACEWEDS_H

#include "rg_Const.h"
#include "TopologicalEntity.h"
#include "rg_dList.h"

class VertexWEDS;
class EdgeWEDS;


class FaceWEDS : public TopologicalEntity
{
protected:
    EdgeWEDS* m_firstEdge;

    rg_BOOL   m_visited;


public:
    FaceWEDS();
    FaceWEDS(const  rg_INT&	ID);
    FaceWEDS(const  rg_INT&	ID, EdgeWEDS* firstEdge);
    FaceWEDS(const  FaceWEDS& face);
    ~FaceWEDS();

    inline EdgeWEDS* getFirstEdge()  const             { return m_firstEdge; };
    inline void      setFirstEdge(EdgeWEDS* firstEdge) { m_firstEdge = firstEdge; };

    inline rg_BOOL   isVisited() const                 { return m_visited; };
    inline void      isVisited(const rg_BOOL& visited) { m_visited = visited; };

    FaceWEDS& operator=(const FaceWEDS& face);

	//  Topological operators
    rg_BOOL isThere(VertexWEDS* vertex) const;
    rg_BOOL isIncidentTo(EdgeWEDS* edge) const;


    rg_INT getBoundingVertices(rg_dList<VertexWEDS*>& boundingVertices) const;
    rg_INT getBoundingEdges(   rg_dList<EdgeWEDS*>&   boundingEdges) const;
    rg_INT getAdjacentFaces(   rg_dList<FaceWEDS*>&   faceList) const;

};

#endif

