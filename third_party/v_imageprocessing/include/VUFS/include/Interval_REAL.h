#ifndef _INTERVAL_REAL_H__
#define _INTERVAL_REAL_H__

#include "rg_Const.h"
#include <float.h>

const rg_REAL REAL_PLUS_INFINITE  = DBL_MAX;
const rg_REAL REAL_MINUS_INFINITE = -DBL_MAX;


class Interval_REAL
{
private:
	rg_REAL m_upperValue;
	rg_REAL m_lowerValue;

public:

    Interval_REAL();
	Interval_REAL(const rg_REAL& lowerValue, const rg_REAL& upperValue);
	Interval_REAL(const Interval_REAL& interval);
	~Interval_REAL();
	
	inline rg_REAL getUpperValue() const { return m_upperValue; }
	inline rg_REAL getLowerValue() const { return m_lowerValue; }
	
	void setUpperValue(const rg_REAL& value);
	void setLowerValue(const rg_REAL& value);
    void setLowerAndUpperValue(const rg_REAL& lowerValue, const rg_REAL& upperValue);

 
    rg_BOOL isInIntervalWithOpenOpenEnd(const rg_REAL& value) const;
    rg_BOOL isInIntervalWithOpenCloseEnd(const rg_REAL& value) const;
    rg_BOOL isInIntervalWithCloseOpenEnd(const rg_REAL& value) const;
    rg_BOOL isInIntervalWithCloseCloseEnd(const rg_REAL& value) const;

	Interval_REAL& operator =(const Interval_REAL& interval);
};

#endif

