#ifndef _DISC_H
#define _DISC_H

#include "rg_Circle2D.h"
#include "TopologicalEntity.h"


namespace V {
namespace GeometryTier {


class Disc : public TopologicalEntity
{
private:
    rg_Circle2D m_circle;
    void*       m_property;

public:
    Disc();
    Disc(const rg_INT& ID);
    Disc(const rg_INT& ID, const rg_Circle2D& circle, void* discProperty = rg_NULL);
    Disc(const Disc& disc);
    ~Disc();

    rg_Circle2D getGeometry() const;
    void*       getProperty() const;

    void        setGeometry(const rg_Circle2D& circle);
    void        setProperty(void* discProperty);

    Disc&       operator=(const Disc& disc);
};


} // GeometryTier
} // V


#endif

