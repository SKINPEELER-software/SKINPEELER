/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_BzCurve2D.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_BzCurve2D 
//           which define Bezier rg_Curve and its property. 
//                          
//	  CLASS NAME  : rg_BzCurve2D
//
//    BASE CLASS  : rg_Curve
//      
//    AUTHOR      : Lee, Soon-Woong
//    START DATE  : 8 Jun 1997    
//
//    HISTORY     :
//
//           1.By Lee, Soon-Woong On 18. Aug. 1997.
//                   make rg_Polynomial* convertBzCurve2Polynomial() const;
//           2.By Jang, Tae-Bum On 1998. 2. 4.
//                   insert void raiseDegree();
//           3.By Ryu, Jung-hyun On 13. Mar. 1998
//                   make rg_Point2D blossomAt(const rg_PARAMETER* parameterList) const;
//			 4.By Ryu, Jung-hyun On 13. Apr. 1998
//					 make rg_BzCurve2D(const rg_BzCurve3D &curve) : degree(curve.degree);
//			 5.By Cho, Youngsong On 22. Apr. 1999
//					 make rg_REAL* inflectionPointByHodograph(rg_INT& numIPts) const;
//
//           Copyright ⓒ 1996 by CAD/CAM Lab. in Hanyang University
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_BZCURVE2D_H
#define _RG_BZCURVE2D_H

#include "rg_Const.h"
#include "rg_Curve.h"
//#include "rg_BzCurve3D.h"
#include "rg_Point2D.h"
#include "rg_Polynomial.h"

class rg_BzCurve2D : public rg_Curve
{
protected:
	rg_DEGREE   degree;
	rg_Point2D* ctrlPts;

public:
	rg_BzCurve2D();
	explicit rg_BzCurve2D(const rg_DEGREE &n);
	rg_BzCurve2D(const rg_DEGREE &n, const rg_Point2D* ctrlpt);
	rg_BzCurve2D(const rg_BzCurve2D &curve);
//	rg_BzCurve2D(const rg_BzCurve3D& curve);
	~rg_BzCurve2D();

	//Operations
	rg_Point2D    evaluatePt(const rg_PARAMETER& t) const;
	rg_BzCurve2D  makeDerivative() const;
	rg_Point2D**  deCasteljau(const rg_PARAMETER& t);
	static rg_REAL bernstein(const rg_DEGREE& i, const rg_DEGREE& n, const rg_PARAMETER& t) ;
	rg_Polynomial bernstein(const rg_DEGREE& n, const rg_INDEX& i) const;
	//History 3.
	rg_Point2D    blossomAt(const rg_PARAMETER* parameterList, const rg_REAL& lowerBound = 0.0, const rg_REAL& upperBound = 1.0) const;


	//Access elements
	inline rg_DEGREE   getDegree() const;                    //CYSONG [Feb05, 20]: add inline
	inline rg_DEGREE   getOrder()  const;                    //CYSONG [Feb05, 20]: add inline
	inline rg_Point2D  getCtrlPt(const rg_DEGREE& i) const;  //CYSONG [Feb05, 20]: add inline
	rg_Point2D* getCtrlPts() const;
	
	void     setDegree(const rg_DEGREE& i);
	void     setOrder(const rg_DEGREE& tOrder);
	void     setCtrlPt(const rg_INDEX& i, const rg_Point2D& point);
	void     setCtrlPts(const rg_Point2D* point);
	void     setCurve(const rg_BzCurve2D& curve);
	rg_BzCurve2D& operator=(const rg_BzCurve2D& curve);
    rg_Polynomial* convertBzCurve2Polynomial() const;
	// rg_CurveSurfaceFunc::bezierToPowerMatrix()를 이용해서 구하는 함수를 추가해야 한다.

    // History 2
    void     raiseDegree(const rg_DEGREE& raisingTimes);

	rg_REAL* inflectionPointByHodograph(rg_INT& numIPts) const;

};


inline rg_DEGREE rg_BzCurve2D::getDegree() const
{
    return degree;
}

inline rg_DEGREE   rg_BzCurve2D::getOrder()  const
{
    return degree + 1;
}

inline rg_Point2D rg_BzCurve2D::getCtrlPt(const rg_DEGREE &i) const
{
    return ctrlPts[i];
}

#endif


