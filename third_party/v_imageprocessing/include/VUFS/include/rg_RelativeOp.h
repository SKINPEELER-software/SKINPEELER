#ifndef _relativeOp
#define _relativeOp

#include "rg_Const.h"

// the following functions use default argument: 
// since the default argument should not be redefined
// the above #ifndef ... #define ... #endif
// have been used.

rg_REAL   rg_ABS(const rg_REAL& x);
rg_INT    rg_EQ(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_GT(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_LT(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_GE(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_LE(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_NE(const rg_REAL& r1, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_ZERO(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_POS(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_NEG(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_NZERO(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_NPOS(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_NNEG(const rg_REAL& r, const rg_REAL& res=rg_MATH_RES); 
rg_INT    rg_BTOR(const rg_REAL& left, const rg_REAL& value, const rg_REAL& right, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_BTORexclusive(const rg_REAL& left, const rg_REAL& value, const rg_REAL& right, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_BTR(const rg_REAL& r1, const rg_REAL& r, const rg_REAL& r2, const rg_REAL& res=rg_MATH_RES);
rg_INT    rg_COMPARE(const rg_REAL& v1, const rg_REAL& v2, const rg_REAL& res=rg_MATH_RES);

rg_REAL	   rg_Max( rg_REAL const& first, rg_REAL const& second);
rg_REAL	   rg_Min( rg_REAL const& first, rg_REAL const& second);

template<class T>
T rg_Min(const T& v1, const T& v2)
{
   return ((v1<v2) ? v1 : v2);
}

template<class T>
T rg_Max(const T& v1, const T& v2)
{
   return ((v1>v2) ? v1 : v2);
}


#endif


