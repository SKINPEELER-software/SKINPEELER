/////////////////////////////////////////////////////////////////////
//
//    FILENAME    : rg_ComplexNumber.h
//    
//    DESCRIPTION : 
//           This is the interface of the class rg_ComplexNumber 
//           which define complex number and its property. 
//                          
//    CLASS NAME  : rg_ComplexNumber
//
//    BASE CLASS  : None
//      
//    AUTHOR      : Lee, Soon-Woong
//    START DATE  : 18 Dev 1996    
//
//           Copyright ¨Ï 1996 by CAD/CAM Lab. in Hanyang University
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_COMPLEXNUMBER_H
#define _RG_COMPLEXNUMBER_H

#include "rg_Const.h"
#include "rg_RelativeOp.h"

#include <iostream>
using namespace std;

class rg_ComplexNumber
{
private:
    rg_REAL realNumber;
    rg_REAL imaginaryNumber;

public:
    rg_ComplexNumber();
    rg_ComplexNumber(const rg_REAL &r, const rg_REAL &i);
    rg_ComplexNumber(const rg_ComplexNumber &other);
    ~rg_ComplexNumber();

    void setRealNumber(const rg_REAL &r);
    void setImaginaryNumber(const rg_REAL &i);
    void setComplexNumber(const rg_ComplexNumber &other);

    rg_REAL getRealNumber() const;
    rg_REAL getImaginaryNumber() const;
    rg_ComplexNumber getComplexNumber() const;
        
    rg_INT  isZero() const;
    rg_INT  isPureRealNumber() const;

    //operator overloading
    rg_ComplexNumber operator +(const rg_ComplexNumber &other);
    rg_ComplexNumber operator -(const rg_ComplexNumber &other);
    rg_ComplexNumber operator *(const rg_ComplexNumber &other);
    rg_ComplexNumber operator /(const rg_ComplexNumber &other);

    rg_ComplexNumber operator +(const rg_REAL &r);
    rg_ComplexNumber operator -(const rg_REAL &r);
    rg_ComplexNumber operator *(const rg_REAL &r);
    rg_ComplexNumber operator /(const rg_REAL &r);
    rg_INT           operator==(const rg_ComplexNumber &other);

    //unary operator
    friend rg_ComplexNumber operator -(const rg_ComplexNumber &other);

    friend rg_ComplexNumber operator *(const rg_REAL &r, rg_ComplexNumber &other);
    friend rg_ComplexNumber operator /(const rg_REAL &r, rg_ComplexNumber &other);

    friend ostream&      operator<<(ostream& os, const rg_ComplexNumber &other);
};

#endif  // rg_ComplexNumber class


