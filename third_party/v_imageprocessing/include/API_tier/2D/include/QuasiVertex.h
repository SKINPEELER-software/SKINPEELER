#ifndef V2D_QUASIVERTEX_H
#define V2D_QUASIVERTEX_H

//#include "DiskGenerator.h"
#include <list>
#include <memory>

#include "rg_Point2D.h"
using std::list;
using std::unique_ptr;

namespace V {

class QuasiTrgl;
class DelaunayTrgl;
class BetaCplx;
class AlphaCplx;
class QuasiFace;
class QuasiEdge;

namespace CoreTier {
class QTVertex2DCore;
}
using namespace CoreTier;

class QuasiVertex {
   private:
    unique_ptr<QTVertex2DCore> m_vertex;

    friend class QuasiTrgl;
    friend class DelaunayTrgl;
    friend class QuasiFace;
    friend class QuasiEdge;
    friend class BetaCplx;
    friend class AlphaCplx;

   public:
    QuasiVertex();
    QuasiVertex(const QTVertex2DCore* const vertexptr);
    QuasiVertex(const QuasiVertex& vertex);
    ~QuasiVertex();

    QuasiVertex& operator=(const QuasiVertex& vertex);
    bool operator==(const QuasiVertex& vertex) const;
    bool operator!=(const QuasiVertex& vertex) const;
    bool operator<(const QuasiVertex& vertex) const;

    rg_Point2D get_point() const;

    list<QuasiVertex> find_adjacent_quasi_vertices() const;
    list<QuasiEdge> find_incident_quasi_edges() const;
    list<QuasiFace> find_incident_quasi_faces() const;

    bool is_member_of(const QuasiFace& face) const;
    bool is_member_of(const QuasiEdge& edge) const;
    bool is_adjacent_to(const QuasiVertex& vertex) const;

    bool is_visited();
    void set_visited(const bool& visited);

    double get_radius();
};
}  // namespace V

#endif
