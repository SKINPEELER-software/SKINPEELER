/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_ImplicitEquation.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_ImplicitEquation
//                         which represents the inplicit form of curve
//                          
//	  CLASS NAME  : rg_ImplicitEquation
//
//    BASE CLASS  : None
//      
//
//    AUTHOR      : Tae-Bum Jang, Soon-Woong Lee
//
//    START DATE  :  8.18.1997    
//
//    HISTORY     :
//
//          By Lee, Soon-Woong, 18 Aug. 1997
//				make constructor : rg_ImplicitEquation(const rg_DEGREE& tDegree,
//                                                  const rg_REAL* coeff);
//          By Lee, Soon-Woong, 5 Sep. 1997
//              make function : rg_REAL substitute(const rg_REAL& x
//                                              const rg_REAL& y) const;
//          By Ryu, Jung-Hyun,  Feb. 17. 1998
//              make operator : rg_REAL* operator[](const rg_INDEX& index);
//
//          By Ryu, Jung-Hyun,  Feb. 19. 1998
//              make operator : rg_ImplicitEquation operator*(const rg_ImplicitEquation& operand);
//              update method: rg_REAL rg_ImplicitEquation::getCoeff( const rg_INT& powerX, const rg_INT& powerY ) const
//              make operator : rg_ImplicitEquation operator+=(const rg_ImplicitEquation& temp) const
//
//          By Ryu, Jung-Hyun,  Feb. 21. 1998
//              update method : rg_ImplicitEquation  operator+( const rg_ImplicitEquation&   temp ) const;
//                              rg_ImplicitEquation  operator-( const rg_ImplicitEquation&   temp ) const;
//	                            rg_ImplicitEquation  operator*( const rg_ImplicitEquation& operand) const;
//
//          By Ryu, Jung-Hyun,  Mar. 6. 1998
//	            rg_ImplicitEquation  operator^( const rg_DEGREE index) const;
//
//			By Ryu, Jung-Hyun,  July. 14. 1998
//				void reconstruct();
//
//          By Jung-Hyun Ryu,  July 15. 1998
//				rg_REAL evaluateImpEquation(const rg_REAL& valueForX, const rg_REAL& valueForY) const;
//
//          By Song, Chanyoung, Sep. 18. 2017
//              update operator : rg_ImplicitEquation& operator=(const rg_ImplicitEquation& operand);
//
/////////////////////////////////////////////////////////////////////


//  This is for implicit equation
//  The coefficients of implicit equation are stored as follows.
//       the coeffient[i][j] stores 
//                          the coeffcient of x^i * y ^j.

#ifndef _RG_IMPLICITEQUATION_H
#define _RG_IMPLICITEQUATION_H

#include "rg_Const.h"
#include "rg_ComplexNumber.h"
#include "rg_Polynomial.h"

class rg_ImplicitEquation
{
private:
    rg_REAL **coefficient;
    rg_DEGREE degree;
    void removeAll();

public:

//// constructors and destructor         //////////////////////                  
    rg_ImplicitEquation();
    rg_ImplicitEquation( const           rg_DEGREE& tDegree);
    rg_ImplicitEquation( const           rg_DEGREE& tDegree,
                      const           rg_REAL* coeff);
    rg_ImplicitEquation( const rg_ImplicitEquation& temp );
    ~rg_ImplicitEquation();

//// get functions                       //////////////////////
    rg_INT    getSizeOfCoeff() const;
    rg_REAL*  getAllCoeff() const;
    rg_REAL   getCoeff( const rg_INT& powerX,
                     const rg_INT& powerY ) const;
    rg_DEGREE getDegree() const;
	rg_REAL evaluateImpEquation(const rg_REAL& valueForX, const rg_REAL& valueForY) const;

//// set functions                       //////////////////////
    void setAllCoeff( const rg_REAL*   tCoeff,
                      const rg_INT       size );
    void setCoeff( const rg_INT&  powerX,
                   const rg_INT&  powerY,
                   const rg_REAL& tCoeff );
    void setDegree( const rg_DEGREE& tDegree);
	void reconstruct();

//// function for substitutions          //////////////////////
    rg_Polynomial substitute( const rg_Polynomial& polynomialX,
                           const rg_Polynomial& polynomialY ) const;
    rg_REAL       substitute( const rg_REAL& x,
                           const rg_REAL& y) const;
//// overloading operator
    rg_ImplicitEquation  operator+( const rg_ImplicitEquation&   temp ) const;
    rg_ImplicitEquation& operator+=( const rg_ImplicitEquation&   temp );
    rg_ImplicitEquation  operator-( const rg_ImplicitEquation&   temp ) const;
    rg_ImplicitEquation  operator*( const rg_REAL&             scalar ) const;
	rg_ImplicitEquation  operator*( const rg_ImplicitEquation& operand) const;
	rg_ImplicitEquation  operator^( const rg_DEGREE index) const;
    rg_ImplicitEquation& operator=( const rg_ImplicitEquation&   temp ) ;
	rg_REAL* operator[](const rg_INDEX& index) ;
	
//// auxilary fuctions to test          //////////////////////
    void show() const;

//// friend functions and classes       //////////////////////
    friend rg_ImplicitEquation  operator*( const rg_REAL&             scalar,
                                        const rg_ImplicitEquation&   temp );


};

#endif

