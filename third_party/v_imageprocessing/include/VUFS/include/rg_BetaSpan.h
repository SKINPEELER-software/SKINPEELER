#ifndef _RG_BETASPAN_
#define _RG_BETASPAN_

#include "rg_Const.h"
#include "Interval_REAL.h"
#include "ConstForBetaComplex.h"

#include <fstream>
using namespace std;

// const rg_INT EXTRANEOUS_SIMPLEX = -1;
// const rg_INT SINGULAR_SIMPLEX   = 0;
// const rg_INT REGULAR_SIMPLEX    = 1;
// const rg_INT INTERIOR_SIMPLEX   = 2;


class rg_BetaSpan
{
private:
    // perf/2a-lite (2026-10-29): small-buffer optimization. Every caller in
    // the 2D image-processing path (VertexBU2D/EdgeBU2D/FaceBU2D::
    // computeBetaSpan*()) needs at most 4 intervals; 3D callers (BetaFace/
    // BetaVertex/BetaEdge) go up to 5. Up to INLINE_CAPACITY intervals are
    // stored inline (no heap allocation at all); anything larger falls back
    // to the original new[]/delete[] behavior transparently. Public API,
    // returned pointer semantics (getBoundingStates()/getBetaIntervals()),
    // and every other member function are unchanged either way.
    enum { INLINE_CAPACITY = 4 };

    rg_INT   m_numBetaInterval;
    rg_INT*  m_boundingState;
    rg_REAL* m_betaInterval;

    rg_INT   m_inlineBoundingState[INLINE_CAPACITY];
    rg_REAL  m_inlineBetaInterval[INLINE_CAPACITY + 1];
    rg_BOOL  m_usesHeapStorage;

    //  edge, not in CH(A), unattached:
    //  m_numBetaInterval = 3;
    //  m_boundingState = new rg_INT[m_numBetaInterval];
    //  m_betaInterval  = new rg_REAL[m_numBetaInterval+1];

    // Point m_boundingState/m_betaInterval at inline storage (numInterval
    // <= INLINE_CAPACITY) or freshly new[]'d heap storage (numInterval >
    // INLINE_CAPACITY) and set m_usesHeapStorage accordingly. Values are
    // left uninitialized -- callers fill them in right after, exactly as
    // the pre-SBO code did with its new[] arrays.
    void allocateStorage(const rg_INT& numInterval);

    // Free heap storage if m_usesHeapStorage is set, then null out both
    // pointers and clear the flag. Always safe to call (no-op on inline/
    // empty storage).
    void releaseStorage();

public:
    rg_BetaSpan();
    rg_BetaSpan(const rg_INT& numInterval);
    rg_BetaSpan(const rg_BetaSpan& span);
    ~rg_BetaSpan();

    rg_INT   getNumBetaInterval() const;
    rg_INT   getBoundingState(const rg_INT& i) const;
    rg_INT*  getBoundingStates() const;
    rg_REAL* getBetaIntervals() const;
    Interval_REAL getBetaInterval(const rg_INT& i) const;
    rg_REAL  getLowerValueOfInterval(const rg_INT& i) const;
    rg_REAL  getUpperValueOfInterval(const rg_INT& i) const;

    rg_BOOL findBetaIntervalOfGivenState(const rg_INT& state, Interval_REAL& interval) const;

    Interval_REAL getExposureInterval() const;
    Interval_REAL getBetaIntervalOfSingularRegularState() const;

    void setNumBetaInterVal(const rg_INT& numInterval);
    void setBetaInterval(const rg_INT& i, const rg_REAL& lower, const rg_REAL& upper);
    void setBetaInterval(const rg_INT& i, const Interval_REAL& interval);
    inline void setBetaInterval(const rg_INT& i, const rg_INT& boundingState, const rg_REAL& lower, const rg_REAL& upper) {
        if ( i>=0 && i<m_numBetaInterval ) {
            m_boundingState[i]  = boundingState;
            m_betaInterval[i]   = lower;
            m_betaInterval[i+1] = upper;
        }
    }

    void setBetaInterval(const rg_INT& i, const rg_INT& boundingState, const Interval_REAL& interval);

	// By JHRYU
	void shiftBetaSpan(const rg_REAL& delta);

    rg_BetaSpan& operator=(const rg_BetaSpan& span);

    rg_INT   findBoundingState(const rg_REAL& beta) const;
    
    rg_BetaSpan merge(const rg_BetaSpan& thatSpan) const;
    void     unify(const rg_BetaSpan& span);


    void     report(ofstream& fout);
};


#endif


