#ifndef V2D_QUASIEDGE_H
#define V2D_QUASIEDGE_H

#include <list>
#include <memory>
using std::list;
using std::unique_ptr;

namespace V {

class QuasiTrgl;
class DelaunayTrgl;
class BetaCplx;
class AlphaCplx;
class QuasiFace;
class QuasiVertex;

namespace CoreTier {
class QTEdge2DCore;
}
using namespace CoreTier;

class QuasiEdge {
   private:
    unique_ptr<QTEdge2DCore> m_edge;

    friend class QuasiTrgl;
    friend class DelaunayTrgl;
    friend class QuasiFace;
    friend class QuasiVertex;
    friend class BetaCplx;
    friend class AlphaCplx;

   public:
    QuasiEdge();
    QuasiEdge(const QTEdge2DCore* const edgeptr);
    QuasiEdge(const QuasiEdge& edge);
    ~QuasiEdge();

    QuasiVertex get_start_quasi_vertex() const;
    QuasiVertex get_end_quasi_vertex() const;
    QuasiVertex get_opposite_quasi_vertex(const QuasiVertex& vertex) const;

    QuasiEdge& operator=(const QuasiEdge& edge);
    bool operator==(const QuasiEdge& edge) const;
    bool operator!=(const QuasiEdge& edge) const;
    bool operator<(const QuasiEdge& edge) const;

    list<QuasiVertex> find_bounding_quasi_vertices() const;
    list<QuasiEdge> find_adjacent_quasi_edges() const;
    list<QuasiFace> find_incident_quasi_faces() const;

    bool is_member_of(const QuasiFace& face) const;
    bool is_adjacent_to(const QuasiEdge& edge) const;
    bool is_incident_to(const QuasiVertex& vertex) const;

    //hwkim
    int get_bounding_state(const double& beta) const;
};

}  // namespace V

#endif