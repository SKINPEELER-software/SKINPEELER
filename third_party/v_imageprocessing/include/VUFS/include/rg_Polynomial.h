/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_Polynomial.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_Polynomial 
//           which define the polynomial and its property. 
//                          
//	  CLASS NAME  : rg_Polynomial
//
//    BASE CLASS  : None
//      
//    AUTHOR      : Lee, Soon-Woong
//    START DATE  : 8 Jan 1997    
//
//    HISTORY     :
//          By Young-Song Cho,  13 Jul. 1997
//              make : rg_REAL  rg_Polynomial::getCoefficient(const rg_INDEX &i) const
//              make : rg_REAL* rg_Polynomial::getCoefficient() const
//              make : void  rg_Polynomial::setCoefficient(const rg_INDEX &i, 
//                                                      const rg_REAL &coeff)
//
//          By Lee, Soon-Woong, 22 Jul. 1997
//				make constructor rg_Polynomial(const rg_DEGREE &dg)
//              make operator overloading *
//              make operator overloading +
//              make operator overloading =
//              make rg_Polynomial rg_Polynomial::power(const rg_INT &i)
//              make rg_ComplexNumber* rg_Polynomial::solve()
//
//          By Hyung-Joo Lee, 15 Jan. 1998
//              make void rg_Polynomial::setCoefficient( const rg_REAL* coeff )
//              make rg_Polynomial rg_Polynomial::makeDerivative() const
//
//          By Jung-Hyun Ryu,  Feb 26. 1998
//              update : rg_REAL rg_Polynomial::getCoefficient(const rg_INDEX &i) const
//           
//          By Jung-Hyun Ryu,  Mar 6. 1998
//              make   : rg_Polynomial  operator ^(const rg_DEGREE index) const
//              make   : rg_REAL& rg_Polynomial::operator [](const rg_DEGREE index)
//
//          By Jung-Hyun Ryu,  July 14. 1998
//				void		reconstruct();
//				rg_Polynomial  operator /(const rg_Polynomial &operand) const;
//				rg_Polynomial  operator /(const rg_REAL &n) const;
//
//          By Jung-Hyun Ryu,  July 15. 1998
//				rg_REAL evaluatePolynomial(const rg_REAL& valueForVariable) const;
//
//          By Jung-Hyun Ryu,  July 28. 1998
//			    Update : void setDegree(const rg_DEGREE &dg);
//				Update : rg_Polynomial& operator =(const rg_Polynomial &polynomial);
//
//           Copyright ⓒ 1996 by CAD/CAM Lab. in Hanyang University
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_POLYNOMIAL_H
#define _RG_POLYNOMIAL_H

#include "rg_Const.h"
#include "rg_ComplexNumber.h"

// This is a class about a polynomial.
// For example, if the polynomial of degree 3 is expressed as
// a0 * x^0 + a1 * x^1 + a2 * x^2 + a3 * x^3, 
// member data : degree = 3, 
//               coefficient[0] = a0,
//               coefficient[1] = a1,
//               coefficient[2] = a2,
//               coefficient[3] = a3.

// 최고차항의 계수가 zero인 경우를 고려할 수 있도록 하자.

class rg_Polynomial
{
protected:
	rg_DEGREE  degree;
	rg_REAL   *coefficient;

public:
	rg_Polynomial();
	rg_Polynomial(const rg_DEGREE& dg);
	rg_Polynomial(const rg_DEGREE& dg, rg_REAL* coeff);
	rg_Polynomial(const rg_Polynomial& eq);
	~rg_Polynomial();

	void           setPolynomial(const rg_DEGREE &dg, const rg_REAL *coeff);
	void           setDegree(const rg_DEGREE &dg);
    void           setCoefficient(const rg_INDEX &i, const rg_REAL &coeff);
	void           setCoefficient( const rg_REAL* coeff );
	void		   reconstruct();


	rg_DEGREE         getDegree() const;
    rg_REAL           getCoefficient(const rg_INDEX &i) const;
	rg_REAL*          getCoefficient() const;
	rg_REAL		   evaluatePolynomial(const rg_REAL& valueForVariable) const;

	rg_Polynomial     power(const rg_INT &k) const;

    rg_Polynomial  operator *(const rg_Polynomial &polynomial) const;
    rg_Polynomial  operator /(const rg_Polynomial &operand) const;
	rg_Polynomial  operator ^(const rg_DEGREE index) const;
	rg_Polynomial  operator +(const rg_Polynomial &polynomial) const;
	rg_Polynomial  operator -(const rg_Polynomial &polynomial) const;
	rg_Polynomial  operator *(const rg_REAL &n) const;
	rg_Polynomial  operator /(const rg_REAL &n) const;
	rg_Polynomial& operator =(const rg_Polynomial &polynomial);
	rg_FLAG        operator ==(const rg_Polynomial &operand);

	rg_REAL& operator [](const rg_DEGREE index);

	friend rg_Polynomial operator *(const rg_REAL &n, const rg_Polynomial &polynomial);

	rg_ComplexNumber* solve();
    rg_ComplexNumber* solve(const rg_REAL& RHS);
	rg_Polynomial makeDerivative() const;
	void updateDegree();
	void displayCoefficient(); //test
};

#endif  // rg_Polynomial class


