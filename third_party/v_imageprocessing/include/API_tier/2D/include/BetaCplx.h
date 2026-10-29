#ifndef V2D_BETACOMPLEX_H
#define V2D_BETACOMPLEX_H

#include <list>
#include <vector>

#include "QuasiEdge.h"
#include "QuasiFace.h"
#include "QuasiVertex.h"
#include "VoroConst.h"
using namespace std;

namespace V {

namespace CoreTier {
class BetaComplex2DCore;
}
using namespace CoreTier;

typedef QuasiFace BetaFace;
typedef QuasiEdge BetaEdge;
typedef QuasiVertex BetaVertex;

class BetaCplx {
   protected:
    unique_ptr<BetaComplex2DCore> m_BCCore;

    friend class QuasiTrgl;

   public:
    BetaCplx();
    BetaCplx(const BetaCplx &BC);
    virtual ~BetaCplx();

    double get_beta_value() const;
    void set_beta_value(const double &betaValue);

    BetaCplx &operator=(const BetaCplx &BC);

    void clear();
    BCStatus extract(const QuasiTrgl &QT, const double &betaValue);

    ////////////////////
    //
    //  1. TRAVERSAL QUERY SET.
    //

    EntitySize get_all_beta_faces(list<BetaFace> &faces) const;
    EntitySize get_all_beta_edges(list<BetaEdge> &edges) const;
    EntitySize get_all_beta_vertices(list<BetaVertex> &vertices) const;
    EntitySize number_of_beta_faces() const;
    EntitySize number_of_beta_edges() const;
    EntitySize number_of_beta_vertices() const;

    EntitySize find_beta_edges_on_boundary(list<BetaEdge> &edges) const;
    EntitySize find_beta_vertices_on_boundary(list<BetaVertex> &vertices) const;
    EntitySize number_of_beta_edges_on_boundary() const;
    EntitySize number_of_beta_vertices_on_boundary() const;

    EntitySize find_beta_edges_in_interior(list<BetaEdge> &edges) const;
    EntitySize find_beta_vertices_in_interior(list<BetaVertex> &vertices) const;
    EntitySize number_of_beta_edges_in_interior() const;
    EntitySize number_of_beta_vertices_in_interior() const;

    EntitySize find_beta_edges_with_given_bounding_state(const SimplexState &state, list<BetaEdge> &edges) const;
    EntitySize find_beta_vertices_with_given_bounding_state(const SimplexState &state, list<BetaVertex> &vertices) const;
    EntitySize number_of_beta_edges_with_given_bounding_state(const SimplexState &state) const;
    EntitySize number_of_beta_vertices_with_given_bounding_state(const SimplexState &state) const;

    ////////////////////
    //
    //  for face

    EntitySize find_adjacent_beta_faces(const BetaFace &face, list<BetaFace> &adjacentFaces) const;
    EntitySize find_bounding_beta_edges(const BetaFace &face, list<BetaEdge> &boundingEdges) const;
    EntitySize find_bounding_beta_vertices(const BetaFace &face, list<BetaVertex> &boundingVertices) const;
    EntitySize number_of_adjacent_beta_faces(const BetaFace &face) const;
    EntitySize number_of_bounding_beta_edges(const BetaFace &face) const;
    EntitySize number_of_bounding_beta_vertices(const BetaFace &face) const;

    EntitySize find_adjacent_beta_faces_with_given_bounding_state(const BetaFace &face, const SimplexState &state, list<BetaFace> &adjacentFaces) const;
    EntitySize find_bounding_beta_edges_with_given_bounding_state(const BetaFace &face, const SimplexState &state, list<BetaEdge> &boundingEdges) const;
    EntitySize find_bounding_beta_vertices_with_given_bounding_state(const BetaFace &face, const SimplexState &state, list<BetaVertex> &boundingVertices) const;
    EntitySize number_of_adjacent_beta_faces_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;
    EntitySize number_of_bounding_beta_edges_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;
    EntitySize number_of_bounding_beta_vertices_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;

    ////////////////////
    //
    //  for edge

    EntitySize find_incident_beta_faces(const BetaEdge &edge, list<BetaFace> &incidentFaces) const;
    EntitySize find_adjacent_beta_edges(const BetaEdge &edge, list<BetaEdge> &adjacentEdges) const;
    EntitySize find_bounding_beta_vertices(const BetaEdge &edge, list<BetaVertex> &boundingVertices) const;
    EntitySize number_of_incident_beta_faces(const BetaEdge &edge) const;
    EntitySize number_of_adjacent_beta_edges(const BetaEdge &edge) const;
    EntitySize number_of_bounding_beta_vertices(const BetaEdge &edge) const;

    EntitySize find_incident_beta_faces_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state, list<BetaFace> &incidentFaces) const;
    EntitySize find_adjacent_beta_edges_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state, list<BetaEdge> &adjacentEdges) const;
    EntitySize find_bounding_beta_vertices_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state, list<BetaVertex> &boundingVertices) const;
    EntitySize number_of_incident_beta_faces_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;
    EntitySize number_of_adjacent_beta_edges_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;
    EntitySize number_of_bounding_beta_vertices_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;

    ////////////////////
    //
    //  for vertex

    EntitySize find_incident_beta_faces(const BetaVertex &vertex, list<BetaFace> &incidentFaces) const;
    EntitySize find_incident_beta_edges(const BetaVertex &vertex, list<BetaEdge> &incidentEdges) const;
    EntitySize find_adjacent_beta_vertices(const BetaVertex &vertex, list<BetaVertex> &adjacentVertices) const;
    EntitySize number_of_incident_beta_faces(const BetaVertex &vertex) const;
    EntitySize number_of_incident_beta_edges(const BetaVertex &vertex) const;
    EntitySize number_of_adjacent_beta_vertices(const BetaVertex &vertex) const;

    EntitySize find_incident_beta_faces_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state, list<BetaFace> &incidentFaces) const;
    EntitySize find_incident_beta_edges_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state, list<BetaEdge> &incidentEdges) const;
    EntitySize find_adjacent_beta_vertices_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state, list<BetaVertex> &adjacentVertices) const;
    EntitySize number_of_incident_beta_faces_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;
    EntitySize number_of_incident_beta_edges_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;
    EntitySize number_of_adjacent_beta_vertices_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;

    list<BetaFace> get_all_beta_faces() const;
    list<BetaEdge> get_all_beta_edges() const;
    list<BetaVertex> get_all_beta_vertices() const;
    list<BetaEdge> find_beta_edges_on_boundary() const;
    list<BetaVertex> find_beta_vertices_on_boundary() const;
    list<BetaEdge> find_beta_edges_in_interior() const;
    list<BetaVertex> find_beta_vertices_in_interior() const;
    list<BetaEdge> find_beta_edges_with_given_bounding_state(const SimplexState &state) const;
    list<BetaVertex> find_beta_vertices_with_given_bounding_state(const SimplexState &state) const;
    list<BetaFace> find_adjacent_beta_faces(const BetaFace &face) const;
    list<BetaEdge> find_bounding_beta_edges(const BetaFace &face) const;
    list<BetaVertex> find_bounding_beta_vertices(const BetaFace &face) const;
    list<BetaFace> find_adjacent_beta_faces_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;
    list<BetaEdge> find_bounding_beta_edges_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;
    list<BetaVertex> find_bounding_beta_vertices_with_given_bounding_state(const BetaFace &face, const SimplexState &state) const;
    list<BetaFace> find_incident_beta_faces(const BetaEdge &edge) const;
    list<BetaEdge> find_adjacent_beta_edges(const BetaEdge &edge) const;
    list<BetaVertex> find_bounding_beta_vertices(const BetaEdge &edge) const;
    list<BetaFace> find_incident_beta_faces_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;
    list<BetaEdge> find_adjacent_beta_edges_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;
    list<BetaVertex> find_bounding_beta_vertices_with_given_bounding_state(const BetaEdge &edge, const SimplexState &state) const;
    list<BetaFace> find_incident_beta_faces(const BetaVertex &vertex) const;
    list<BetaEdge> find_incident_beta_edges(const BetaVertex &vertex) const;
    list<BetaVertex> find_adjacent_beta_vertices(const BetaVertex &vertex) const;
    list<BetaFace> find_incident_beta_faces_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;
    list<BetaEdge> find_incident_beta_edges_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;
    list<BetaVertex> find_adjacent_beta_vertices_with_given_bounding_state(const BetaVertex &vertex, const SimplexState &state) const;
    //     EntitySize      find_first_order_neighbor_beta_vertices( const BetaVertex& vertex, list<BetaVertex>& neighborVertices ) const;
    //     EntitySize      find_first_order_neighbor_beta_vertices( const list<BetaVertex>& vertices, list<BetaVertex>& neighborVertices ) const;
    //     EntitySize      find_second_order_neighbor_beta_vertices( const BetaVertex& vertex, list<BetaVertex>& neighborVertices ) const;
    //     EntitySize      find_second_order_neighbor_beta_vertices( const list<BetaVertex>& vertices, list<BetaVertex>& neighborVertices ) const;
    //     EntitySize      number_of_first_order_neighbor_beta_vertices( const BetaVertex& vertex ) const;
    //     EntitySize      number_of_first_order_neighbor_beta_vertices( const list<BetaVertex>& vertices ) const;
    //     EntitySize      number_of_second_order_neighbor_beta_vertices( const BetaVertex& vertex ) const;
    //     EntitySize      number_of_second_order_neighbor_beta_vertices( const list<BetaVertex>& vertices ) const;
    //
    //
    //
    //     ///////////////////////////////////////////////////////////////////////////
    //     //
    //     //  2. BOOLEAN QUERY SET
    //     //
    //
    //     bool            are_there_beta_faces_among( const Circle2D& disk1, const Circle2D& disk2, const Circle2D& disk3, list<BetaFace>& faces ) const;
    //     bool            are_there_beta_edges_among( const Circle2D& disk1, const Circle2D& disk2, list<BetaEdge>& edges ) const;
    //     bool            is_there_BCVertex_of( const Circle2D& disk, BetaVertex& vertex ) const;
    //
    //     bool            are_there_common_beta_faces( const BetaVertex& v1, const BetaVertex& v2, const BetaVertex& v3, list<BetaFace>& faces ) const;
    //     bool            are_there_common_beta_edges( const BetaVertex& v1, const BetaVertex& v2, list<BetaEdge>& edges ) const;
    //
    //     bool            is_on_squeezed_hull( const BetaEdge& edge ) const;
    //     bool            is_on_squeezed_hull( const BetaVertex& vertex ) const;
    //     bool            is_on_boundary( const BetaEdge& edge ) const;
    //     bool            is_on_boundary( const BetaVertex& vertex ) const;
    //     bool            is_in_interior( const BetaEdge& edge ) const;
    //     bool            is_in_interior( const BetaVertex& vertex ) const;
    //
    //     bool            is_incident( const BetaFace& face, const BetaEdge& edge ) const;
    //     bool            is_incident( const BetaFace& face, const BetaVertex& vertex ) const;
    //     bool            is_incident( const BetaEdge& edge, const BetaVertex& vertex ) const;
    //     bool            is_adjacent( const BetaFace& face1, const BetaFace& face2 ) const;
    //     bool            is_adjacent( const BetaEdge& edge1, const BetaEdge& edge2 ) const;
    //     bool            is_adjacent( const BetaVertex& vertex1, const BetaVertex& vertex2 ) const;
    //     bool            is_member( const BetaEdge& edge, const BetaFace& face ) const;
    //     bool            is_member( const BetaVertex& vertex, const BetaFace& face ) const;
    //     bool            is_member( const BetaVertex& vertex, const BetaEdge& edge ) const;
    //
    //     EntitySize      find_BCFace_pair_in_anomaly( list< pair<BetaFace, BetaFace> >& facePair ) const;
    //     EntitySize      number_of_BCFace_pair_in_anomaly() const;
    //     bool            isAnomaly( const BetaFace& face1, const BetaFace& face2 ) const;
    //
    //     SimplexState    bounding_state( const BetaFace& face ) const;
    //     SimplexState    bounding_state( const BetaEdge& edge ) const;
    //     SimplexState    bounding_state( const BetaVertex& vertex ) const;
};

}  // namespace V

#endif
