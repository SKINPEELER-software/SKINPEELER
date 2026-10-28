//******************************************************************************
//
//	  FILENAME    : rg_Curve.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_Curve 
//           which will be the base class of all kinds of curves. 
//                          
//	  CLASS NAME  : rg_Curve
//
//    BASE CLASS  : NONE
//      
//
//    AUTHOR      : Deok-Soo Kim, Young-Song Cho
//    START DATE  : 21 Jun 1996    
//
//            Copyright (c) CAD/CAM Lab. Department of Industrial Engineering
//                          Hanyang University, Seoul Korea    	  	
//
//******************************************************************************

#ifndef _RG_CURVE_H
#define _RG_CURVE_H

#include "rg_Const.h"
//#include "Defconst.h"


class rg_Curve
{
protected:
    unsigned rg_INT ID;
    rg_Planarity    planarOrSpatial;

public:
////    Constructor & Destructor.
    rg_Curve();
    rg_Curve( const unsigned rg_INT &newID, 
           const rg_Planarity &newPlanarity );

    ////  Copy Constructor March 13 1997
    rg_Curve( const rg_Curve &curve );
    virtual ~rg_Curve();

////    Get Functions.
    unsigned rg_INT getID() const;
    rg_Planarity    getPlanarity() const;

////    Set Functions.
    void setID(const unsigned rg_INT &newID);
    void setPlanarity(const rg_Planarity &newPlanarity);
    virtual void removeAll();

////    Abstract Virtual functions
};

#endif


