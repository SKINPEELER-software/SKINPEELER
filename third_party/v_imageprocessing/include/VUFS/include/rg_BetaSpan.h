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
    rg_INT   m_numBetaInterval;
    rg_INT*  m_boundingState;
    rg_REAL* m_betaInterval;
    
    //  edge, not in CH(A), unattached: 
    //  m_numBetaInterval = 3;
    //  m_boundingState = new rg_INT[m_numBetaInterval];
    //  m_betaInterval  = new rg_REAL[m_numBetaInterval+1];

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


