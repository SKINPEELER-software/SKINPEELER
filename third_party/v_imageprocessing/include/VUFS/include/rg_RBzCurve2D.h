/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_RBzCurve2D.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_BzCurve2D 
//           which define rational Bezier rg_Curve and its property. 
//                          
//	  CLASS NAME  : rg_RBzCurve2D
//
//    BASE CLASS  : rg_BzCurve2D
//      
//    AUTHOR      : Lee, Soon-Woong
//    START DATE  : 8 Jun 1997    
//
//    History:
//
//           1. inserted the following function at 1997.11.11 
//                  void setWeight(const rg_INDEX& i,
//                                 const rg_REAL&  newWeight); by Jang, Tae-Bum 
//           2. Inserted the following function at 1998.1.14 
//                  rg_BzCurve2D scaledHodo() const; by Hyung-Joo Lee
//           3. Inserted the following function at 1998.1.15 
//                  rg_Polynomial* numeratorDerivative() const; by Hyung-Joo Lee
//                  rg_Polynomial  denominatorDerivative() const; by Huyng-Joo Lee
//                  rg_Point2D*    inflectionPointByHodograph() const; by Hyung-Joo Lee
//	                rg_INT         countInflectionPointByHodograph() const; by Hyung-Joo Lee
//	                rg_Point2D*    inflectionPointByLeeMethod() const; by Hyung-Joo Lee
//	                rg_INT         countInflectionPointByLeeMethod() const; by Hyung-Joo Lee
//	                rg_Polynomial* convertNumerator2Polynomial() const; by Hyung-Joo Lee
//	                rg_Polynomial  convertDenominatorPolynomial() const; by Hyung-Joo Lee
//
//          4. Inserted the following function at 1998.2. 4.
//                      rg_RBzCurve2D makeDerivative() const; by Jang, Tae-Bum
//
//          5. Inserted the following function at 1998. 7.10
//                      rg_REAL* getWeightVector() const;
//
//          6. Inserted the following function at 1999. 4.21
//                      rg_Point2D* inflectionPointByDM( rg_INT& num ) const;
//				        Find inflection points by DM approach.
//
//				        
//
//                      
//           Copyright ¨Ï 1996 by CAD/CAM Lab. in Hanyang University
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_RBZCURVE2D_H
#define _RG_RBZCURVE2D_H

#include <math.h>
#include "rg_Const.h"
#include "rg_Point2D.h"
#include "rg_BzCurve2D.h"

class rg_RBzCurve2D :public rg_BzCurve2D
{
protected:
	rg_REAL  *weight;

public:
	rg_RBzCurve2D();
	rg_RBzCurve2D(const rg_DEGREE &n);
	rg_RBzCurve2D(const rg_DEGREE &n, const rg_Point2D *ctrlpt);
	rg_RBzCurve2D(const rg_RBzCurve2D &curve);
	~rg_RBzCurve2D();

	//Operations
	rg_Point2D evaluatePt(const rg_PARAMETER &t) const;
	rg_Point2D evaluateDerivative(const rg_PARAMETER &t) const;

	//Access elements
	inline rg_REAL    getWeight(const rg_INDEX &i) const; //CYSONG [Oct17, 19]: add inline
	// History 5
	rg_REAL*   getWeightVector() const;
    void    setDegree(const rg_DEGREE& tDegree);
	void    setWeight(const rg_REAL *w);
    void    setWeight(const rg_INDEX& i,
                      const rg_REAL&  newWeight);// insert in History 1
    void    setCurve(const rg_RBzCurve2D &curve);

	rg_RBzCurve2D& operator=(const rg_RBzCurve2D& curve);

	//History 2
	rg_BzCurve2D scaledHodo() const;
	//History 3
	rg_Polynomial* numeratorDerivative() const;
	rg_Polynomial  denominatorDerivative() const;

	rg_Point2D* inflectionPointByHodograph() const;
	rg_INT countInflectionPointByHodograph() const;

	rg_Point2D* inflectionPointByLeeMethod() const;
	rg_INT countInflectionPointByLeeMethod() const;

	rg_Point2D* inflectionPointByNormalMethod() const;
	rg_INT countInflectionPointByNormalMethod() const;

	rg_REAL* inflectionPointByDM( rg_INT& num ) const;

	rg_Polynomial* convertNumerator2Polynomial() const;
	rg_Polynomial  convertDenominator2Polynomial() const;

    // History 4
    rg_RBzCurve2D makeDerivative() const;

};

inline rg_REAL rg_RBzCurve2D::getWeight(const rg_INDEX &i) const
{
    return weight[i];
}

#endif  // rg_RBzCurve2D class

