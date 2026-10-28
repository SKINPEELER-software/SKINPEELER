#ifndef V2D_QUASIFACE_H
#define V2D_QUASIFACE_H

#include <list>
#include <memory>
using std::list;
using std::unique_ptr;

namespace V {

class QuasiTrgl;
class DelaunayTrgl;
class BetaCplx;
class AlphaCplx;
class QuasiEdge;
class QuasiVertex;

namespace CoreTier {
class QTFace2DCore;
}
using namespace CoreTier;

class QuasiFace {
   private:
    unique_ptr<QTFace2DCore> m_face;

    friend class QuasiTrgl;
    friend class DelaunayTrgl;
    friend class QuasiEdge;
    friend class QuasiVertex;
    friend class BetaCplx;
    friend class AlphaCplx;

   public:
    QuasiFace();
    QuasiFace(const QTFace2DCore* const faceptr);
    QuasiFace(const QuasiFace& face);
    ~QuasiFace();

    QuasiFace& operator=(const QuasiFace& face);
    bool operator==(const QuasiFace& face) const;
    bool operator!=(const QuasiFace& face) const;
    bool operator<(const QuasiFace& face) const;

    list<QuasiVertex> find_bounding_quasi_vertices() const;
    list<QuasiEdge> find_bounding_quasi_edges() const;
    list<QuasiFace> find_adjacent_quasi_faces() const;

    bool is_adjacent_to(const QuasiFace& face) const;
    bool is_incident_to(const QuasiEdge& edge) const;
    bool is_incident_to(const QuasiVertex& vertex) const;

    QuasiVertex find_mate_quasi_vertex(const QuasiEdge& edge) const;
    QuasiEdge find_mate_quasi_edge(const QuasiVertex& vertex) const;
};

}  // namespace V

#endif
