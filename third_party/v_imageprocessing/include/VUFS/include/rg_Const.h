 
#ifndef  _CONST_INCLUDED
#define  _CONST_INCLUDED

#include <limits.h>
#include <float.h>

typedef bool    rg_BOOL;


#define rg_FLAG      short //short int
#define rg_DEGREE    short //short int
#define rg_INDEX     int
#define rg_INT       int
#define rg_FLOAT     float 
#define rg_REAL      double
/*
#ifndef CORE_LEVEL
#   define rg_REAL      double
#elif 
#   define rg_REAL      machine_double
#endif
*/

#define rg_PARAMETER double

//  The following was added by Youngsong Cho on 8. Mar. 2000. 
//#define BOOLEAN   short int
//  End of addition

//The following was added by Lee Soon-Woong on 7.7.1997. 
#define rg_ORDER    short //short int
//End of addition

//  The following was added by Young-Song Cho on 9 Jul. 1997.
//  #ifndef & #endif were added by Dong-Gyou Lee on 4 Mar. 1998. 
#define rg_FALSE   0
#define rg_TRUE    1

//  By Youngsong Cho (2004. 3. 30)
const rg_FLAG rg_UNKNOWN = -1;

#define rg_NULL    0
//  End of addition.

const rg_REAL rg_ROUND = 0.5;
const rg_REAL rg_PI    = 3.14159265358979323846;
const double	DegreeToRad		= 57.2957795130823;

const rg_REAL ANGLE_90_DEGREE  = rg_PI*0.5;
const rg_REAL ANGLE_180_DEGREE = rg_PI;
const rg_REAL ANGLE_270_DEGREE = rg_PI*1.5;
const rg_REAL ANGLE_360_DEGREE = rg_PI*2.0;

const rg_INT  rg_INT_INFINITY  = INT_MAX;
const rg_REAL rg_REAL_INFINITY = DBL_MAX;

//const rg_REAL ROUNDING = 0.5;

////////////////////////////////////////////////////////////
///		TOLERANCES
////////////////////////////////////////////////////////////
const rg_REAL     resNeg20= 1.0e-20 ;
const rg_REAL     resNeg19= 1.0e-19 ;
const rg_REAL     resNeg18= 1.0e-18 ;
const rg_REAL     resNeg17= 1.0e-17 ;
const rg_REAL     resNeg16= 1.0e-16 ;
const rg_REAL     resNeg15= 1.0e-15 ;
const rg_REAL     resNeg14= 1.0e-14 ;
const rg_REAL     resNeg13= 1.0e-13 ;
const rg_REAL     resNeg12= 1.0e-12 ;
const rg_REAL     resNeg11= 1.0e-11 ;
const rg_REAL     resNeg10= 1.0e-10 ;
const rg_REAL     resNeg9= 1.0e-9 ;
const rg_REAL     resNeg8= 1.0e-8 ;
const rg_REAL     resNeg7= 1.0e-7 ;
const rg_REAL     resNeg6= 1.0e-6 ;
const rg_REAL     resNeg5= 1.0e-5 ;
const rg_REAL     resNeg4= 1.0e-4 ;
const rg_REAL     resNeg3= 1.0e-3 ;
const rg_REAL     resNeg2= 1.0e-2 ;
const rg_REAL     resNeg1= 1.0e-1 ;

//Please try to use these semantic tolerance values in the program.
//If there is not an appropriate names defined, please define it here
//and then use the defined name in the program.
const rg_REAL rg_SYSTEM_RES= 1.0e-15;           //system resolution 
const rg_REAL rg_MATH_RES= 1.0e-6;           //mathematical resolution 
//const rg_REAL res1Uncertain= resNeg6;     //most narrow tolerance resolution
//const rg_REAL res2Uncertain= resNeg5;     //looser
//const rg_REAL res3Uncertain= resNeg4;     //looooser
//const rg_REAL res4Uncertain= resNeg3;     //looooooooooooser
//const rg_REAL resVeryUncertain= resNeg3;  //wide resolution

//const rg_REAL newtonRes= rg_MATH_RES;         //tolerance for newton iteration
//const rg_REAL bisectRes= resNeg12;        //tolerance for bisection iteration

//to test if two initial elements' tangent vectors are parallel.
//const rg_REAL resTietvap= resNeg3;
//const rg_REAL resTietvap2= resNeg2;

//for input data 
//const rg_REAL resMassageInput= resNeg3;
//const rg_REAL resMergeInput= resNeg4;

//The following was commented by Young-Song Cho on 7.7.1997. 

const rg_REAL rg_MAX_REAL = 1.0e38; // the largest possible real value 

//End of comment

////////////////////////////////////////
///        Constances
////////////////////////////////////////
//const bufSize = 256;
//const rg_INT BUFSIZE = 256;
//const rg_INT MaxNoOfNewtonIteration= 30;
#define rg_LINEAR       1
#define rg_QUADRATIC    2
#define rg_CUBIC        3
 
////////////////////////////////////////
///        Enumerations
////////////////////////////////////////
enum	rg_Planarity	{ rg_PLANAR, rg_SPATIAL, rg_NONE };

//The following was commented by Lee Soon-Woong on 7.7.1997. 
/*
enum    GEO_TYPE        { Point, rg_Line, Arc, Circle, Elem };
enum    INPUT_POLYGON_TYPE {boundaryPolygon, islandPolygon};

enum    CURVE_TYPE      { LineSeg, ParabolaSeg, EllipseSeg,
                          HyperbolaSeg, CircleSeg, GeneralSeg };
enum    MARCH_DIRECTION {AS_IS, REVERSED};
enum    POINT_LOC       {EXTREME, EXTREME_CENTER_OF_ARC, INTERIOR};
enum    ORIENT          { CW, CCW, None};
enum    REGION_TYPE     { SP_Region, EP_Region, MAIN_Region, SP_Ray, MP_Ray, EP_Ray };
enum    POSITION_TYPE   { ON, Apart_ON, Apart_OUT, Apart_IN };
enum    rg_FLAG         { rg_FALSE, rg_TRUE };
enum    DIRECTION       { SAME, REVERSE };
enum    LFT_OR_RGT      { LEFT, RIGHT, NONE, BOTH, TERMINATION };
enum    MERGE_TYPE      { BOUNDARY_NON_ALL_REFLEX_VTX, 
                          ISLAND_ALL_REFLEX_VTX, 
                          ISLAND_NON_ALL_REFLEX_VTX };
enum    POLYGON_TYPE    { BOUNDARY, ISLAND };
enum    VOR_EDGE_TYPE   { trunk, branch, fluff, fake, none };
enum    OFFSET_STATE    { Start, Split, Close, Connect, Initial };
*/
//End of comment
#endif


