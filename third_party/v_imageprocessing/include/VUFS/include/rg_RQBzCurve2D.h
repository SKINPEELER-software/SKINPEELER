/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_RQBzCurve2D.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_RQBzCurve2D 
//           which define rational quadratic Bezier rg_Curve and its property. 
//                          
//	  CLASS NAME  : rg_RQBzCurve2D
//
//    BASE CLASS  : rg_RBzCurve2D
//      
//    AUTHOR      : Lee, Soon-Woong
//    START DATE  : 8 Jun 1997    
//
//           Copyright ¨Ï 1996 by CAD/CAM Lab. in Hanyang University
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_RQBZCURVE2D_H
#define _RG_RQBZCURVE2D_H

#include <math.h>
#include "rg_Const.h"
#include "rg_Point2D.h"
#include "rg_RBzCurve2D.h"
#include "rg_BoundingBox2D.h"

class rg_RQBzCurve2D :public rg_RBzCurve2D
{
public:
	rg_RQBzCurve2D();
	rg_RQBzCurve2D(const rg_Point2D *ctrlpt);
	rg_RQBzCurve2D(const rg_RQBzCurve2D &curve);
	~rg_RQBzCurve2D();

	//Operations
	void    makeRQBezier(const rg_Point2D& b0, 
                         const rg_Point2D& t0, 
                         const rg_Point2D& b2, 
						 const rg_Point2D& t2,
                         const rg_Point2D& p); 
	void    makeRQBezier(const rg_Point2D& b0, 
						 const rg_Point2D& b1, 
						 const rg_Point2D& b2,
		                 const rg_Point2D& p);

	//Access elements
	rg_REAL        getParameter4Point(const rg_Point2D &point) const;
    rg_BoundingBox2D makeBoundingBox() const;
	rg_RQBzCurve2D& operator=(const rg_RQBzCurve2D& curve);
};
#endif  // rg_RQBzCurve2D class

