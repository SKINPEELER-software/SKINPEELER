#ifndef _LOOP_OF_COMPONENT_H
#define _LOOP_OF_COMPONENT_H

#include "EdgeBU2D.h"
#include <list>
using namespace std;


namespace V {
namespace GeometryTier {


class ComponentBSFace;

class LoopOfComponent
{
private:
    rg_dList<EdgeBU2D*> m_edgesOnLoop;
    bool                m_isOuterLoop;
    ComponentBSFace*    m_component;
    double              m_signedArea;

    double              m_length;

public:
    LoopOfComponent(void);
    LoopOfComponent(const LoopOfComponent& loop);
    ~LoopOfComponent(void);

    bool                        isOuterLoop() const;
    rg_dList<EdgeBU2D*>&        getEdges();
    int                         getNumOfEdgesOnLoop() const;    
    double                      getLength() const;
    ComponentBSFace*            getComponent() const;
    double                      getSignedArea() const;
    void                        setComponent( ComponentBSFace* component );

    void                        findOrientedBoundingPoints( list<rg_Point2D>& boundingPoints );
    void                        findOrientedBoundingVertices(list<VertexBU2D*>& boundingVertices);


    inline void                 addEdge( EdgeBU2D* edge ) { m_edgesOnLoop.add(edge); }
    inline void                 isOuterLoop(const bool& isThisLoopOuter) {m_isOuterLoop = isThisLoopOuter;}


    double                      computeLength();
    double                      computeSignedArea();


};

inline bool                     LoopOfComponent::isOuterLoop() const { return m_isOuterLoop; }
inline rg_dList<EdgeBU2D*>&     LoopOfComponent::getEdges() { return m_edgesOnLoop; }
inline int                      LoopOfComponent::getNumOfEdgesOnLoop() const { return m_edgesOnLoop.getSize(); }
inline double                   LoopOfComponent::getLength() const { return m_length; }
inline ComponentBSFace*         LoopOfComponent::getComponent() const { return m_component; }
inline double                   LoopOfComponent::getSignedArea() const { return m_signedArea; }
inline void                     LoopOfComponent::setComponent( ComponentBSFace* component ) { m_component = component; }

} // GeometryTier
} // V
#endif

