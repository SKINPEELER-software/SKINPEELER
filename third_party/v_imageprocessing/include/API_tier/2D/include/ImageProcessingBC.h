#ifndef V2D_IMAGEPROCESSING_BC_H
#define V2D_IMAGEPROCESSING_BC_H

#include <list>
#include <map>
#include <vector>

#include "BetaCplx.h"
#include "ComponentBSFace.h"
using namespace std;

namespace V {
using namespace GeometryTier;

class ImageProcessingBC : public BetaCplx {
   private:
    rg_dList<ComponentBSFace> m_betaFaceCompononents;
    map<FaceBU2D*, ComponentBSFace*> m_mapOfComponentNBetaFace;
    ComponentBSFace* m_biggestComponentBSFace;

    // Outer-loop length per component, cached during classifyByComponents().
    // Lets the (const) score helper compute outer-loop circularity without
    // calling the non-const getOuterLoop()/computeLength().
    map<ComponentBSFace*, double> m_outerLoopLengthOf;

    // Selection-score metric switch (see set_use_outer_loop_circularity).
    static bool s_useOuterLoopCircularity;

    // Unified selection score for one component, honouring the metric switch.
    // area * circularity^2  (legacy)  OR  area * outer_circularity^2 (default).
    double compute_selection_score(ComponentBSFace* comp) const;

   public:
    ImageProcessingBC();
    ImageProcessingBC(const ImageProcessingBC& IPBC);
    ~ImageProcessingBC();

    list<BetaEdge> get_edge_of_outer_loop_of_biggest_componentBSFace();
    list<BetaVertex> get_vertices_of_biggest_componentBSFace();

    void constructForImageProcessing(const vector<vector<int> >& grayDataSet, const bool& darkerPixelBecomesBiggerDisk) const;
    void constructForImageProcessing16bit(const vector<vector<int>>& grayDataSet, const int bitsStored, const bool& darkerPixelBecomesBiggerDisk) const;
    void classifyByComponents();
    void initialize_all_member();

    // --- Metrics for selected biggest component (call after classifyByComponents) ---
    void   select_biggest_componentBSFace();
    int    get_n_components() const;

    // -------------------------------------------------------------------------
    // Component-selection score metric switch.
    //   false (legacy) : score = area * circularity^2, where circularity uses
    //                     the TOTAL boundary length (outer loop + all inner-hole
    //                     rims). Holes inflate the perimeter and depress the
    //                     circularity of a coherent body, occasionally letting a
    //                     small round fragment outscore the true body.
    //   true  (default): score = area * outer_circularity^2, where
    //                     outer_circularity = 2*sqrt(pi*area) / L_outer uses ONLY
    //                     the outer-loop length. Hole rims no longer penalise the
    //                     body; elongated bed/spine still scores low because a
    //                     long thin shape has a long outer loop.
    // Static so the default applies uniformly across every ImageProcessingBC
    // instance (GUI / web server / CLI) without per-call wiring.
    // -------------------------------------------------------------------------
    static void set_use_outer_loop_circularity(bool on);
    static bool get_use_outer_loop_circularity();
    int    get_n_beta_faces_of_biggest_component();
    double get_area_of_biggest_component() const;
    double get_circularity_of_biggest_component() const;
    double get_outer_circularity_of_biggest_component() const;
    int    get_n_inner_loops_of_biggest_component();
    int    get_n_outer_edges_of_biggest_component();

    // -------------------------------------------------------------------------
    // BedDetector support API (additive — existing API unchanged)
    // Call after classifyByComponents().
    // -------------------------------------------------------------------------

    // Per-component info: id, centroid, area, circularity, score (area * circ^2).
    // Returned vector is sorted by score descending (index 0 = biggest/body0).
    struct ComponentInfo {
        int    id;
        double centroid_x;
        double centroid_y;
        double area;
        double circularity;
        double score;   // area * circularity^2 (same metric as biggest-component selection)
        // --- geometry diagnostics (populated only by enumerate_components_with_geometry) ---
        double perimeter_total   = 0.0;  // Σ length of ALL regular boundary edges (outer + inner-hole rims); same metric as computeCircularity
        double outer_loop_length = 0.0;  // length of the OUTER loop only (getOuterLoop()->computeLength())
        int    n_inner_loops     = 0;    // number of inner (hole) loops
        int    n_outer_edges     = 0;    // number of edges on the outer loop
    };

    // Enumerate all VD components. Must call classifyByComponents() first.
    // Returns score-descending sorted vector (index 0 = current body0 candidate).
    std::vector<ComponentInfo> enumerate_components() const;

    // Diagnostic variant: same as enumerate_components() but also fills the
    // geometry fields (perimeter_total, outer_loop_length, n_inner_loops,
    // n_outer_edges). Non-const because it calls ComponentBSFace::computePerimeter()
    // (sets m_perimeter) and LoopOfComponent::computeLength() (sets m_length).
    // Existing enumerate_components() and BedDetector path are unaffected.
    std::vector<ComponentInfo> enumerate_components_with_geometry();

    // Return outer-loop edge list for the component matching component_id.
    // If found, m_biggestComponentBSFace is updated to that component
    // (downstream get_* functions remain compatible).
    // Returns empty list if component_id is not found.
    list<BetaEdge> get_edge_of_outer_loop_of_componentBSFace(int component_id);

    // -------------------------------------------------------------------------
    // biggest_component_pixel_mask (component-only fast path for Jaccard)
    // Call after classifyByComponents() + select_biggest_componentBSFace().
    // Converts all vertices of the biggest beta-complex component to pixel
    // coordinates using the standard Voronoi→image transform:
    //   col = vx + imageWidth/2  - coordOffset
    //   row = vy + imageHeight/2 - coordOffset
    // Returns a flat H*W vector<char> (row-major), 1 = body pixel, 0 = background.
    // Must call select_biggest_componentBSFace() (or
    // get_edge_of_outer_loop_of_biggest_componentBSFace()) before this.
    // -------------------------------------------------------------------------
    std::vector<char> biggest_component_pixel_mask(
        int imageHeight, int imageWidth, double coordOffset) const;

    // -------------------------------------------------------------------------
    // reset_vertex_visited — reset visited flags on ALL VD vertices.
    //
    // Shell peeling traversal (calculateInnerShellsAdaptive / exp_beta_sweep)
    // sets is_visited() on QuasiVertex objects to mark already-collected
    // vertices.  When the same ImageProcessingBC is reused across β values
    // (1-Voronoi β-sweep), these flags must be cleared before the next shell
    // extraction pass, otherwise previously-visited vertices are skipped and
    // shells come out empty.
    //
    // classifyByComponents() resets face/edge visited flags but NOT vertex
    // visited flags — so this method must be called explicitly before each
    // shell peeling pass in a multi-β loop.
    //
    // Usage in exp_beta_sweep:
    //   bc.set_beta_value(beta);
    //   bc.classifyByComponents();
    //   bc.select_biggest_componentBSFace();
    //   bc.reset_vertex_visited();   // <-- clear before shell traversal
    //   auto outerEdges = bc.get_edge_of_outer_loop_of_biggest_componentBSFace();
    //   // ... shell peeling loop ...
    // -------------------------------------------------------------------------
    void reset_vertex_visited();

    // -------------------------------------------------------------------------
    // extract_shell_radius_stats — leak-free shell peeling for the β-sweep.
    //
    // Performs the same shell expansion as the experiment's compute_shell_stats
    // (outer loop = shell[0], then BFS over REGULAR/INTERIOR β-edges) but uses
    // the raw Voronoi pointers (VertexBU2D* / EdgeBU2D*) directly instead of the
    // QuasiVertex / QuasiEdge wrappers.
    //
    // The wrapper accessors (QuasiEdge::get_start_quasi_vertex,
    // QuasiVertex::find_incident_quasi_edges, ...) each `new` a QT*2DCore and
    // pass it to a QuasiVertex/QuasiEdge constructor that *copies* it into a
    // unique_ptr WITHOUT taking ownership of the original — so every such call
    // leaks one core object. In the 1-Voronoi β-sweep this fires millions of
    // times (per incident edge × per vertex × per shell × 31 β × N slices),
    // producing the observed per-slice RSS growth. Walking the raw VD pointers
    // here performs zero heap allocation per vertex/edge and removes the leak.
    //
    // Pre-conditions (same as before): set_beta_value(beta), classifyByComponents(),
    // select_biggest_componentBSFace(), reset_vertex_visited() called in order.
    //
    // Each ShellRadiusStat holds raw accumulators; the caller derives
    // mu = 2*mean(r), sigma = 2*std(r). shells_out[0] = outermost shell.
    // maxShells caps the number of shells (pass ADAPTIVE_SHELL_MAX).
    // -------------------------------------------------------------------------
    struct ShellRadiusStat {
        int    n_vertices = 0;
        double sum_r      = 0.0;
        double sum_r2     = 0.0;
    };
    void extract_shell_radius_stats(double beta, int maxShells,
                                    std::vector<ShellRadiusStat>& shells_out);

   private:
    void propagateComponent(FaceBU2D* seedFace);
};

}  // namespace V
#endif