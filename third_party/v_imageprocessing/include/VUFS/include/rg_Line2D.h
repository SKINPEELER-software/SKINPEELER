//********************************************************************
//
//	  FILENAME    : rg_Line2D.h
//	  
//    DESCRIPoint2DION : 
//           This consists of the interface of class rg_Line2D.      
//
//    BASE CLASS  : None  
//
//    AUTHOR      : Deok-Soo Kim, Young-Song Cho
//    START DATE  : 7 Apr 1998    
//
//    HISTORY:
//            
//        [2020.03.31] Chanyoung Song merged LineSegment2D into rg_Line2D.
//            NOTE the mapping table which includes only a confusing part of the whole functions. 
//              1) direction()          -->  evaluateVector() 
//                         (direction() returns unit vector but evaluateVector() does not)
//              2) evaluate_parameter() -->  get_parameter_of_point_on_line_segment()
//                         (The body of 'get_parameter_of_point_on_line_segment' is changed to 'evaluate_parameter'.
//              3) project_point()      -->  project()
//              4) does_contain()       -->  does_contain_in_line_segment()
//              5) does_contain_in_corresponding_line() --> does_contain()
//              6) does_intersect()     -->  does_intersect_as_line_segment()
//              7) intersect()          -->  compute_intersection_btw_two_line_segments() 
//
//            Copyright (c) CAD/CAM Lab.    	  	
//
//*********************************************************************

#ifndef _RG_LINE2D_H
#define _RG_LINE2D_H

#include "rg_Const.h"
#include "rg_Point2D.h"
#include "rg_RelativeOp.h"
#include <math.h>

class rg_Line2D
{
public:
    enum IntersectionPointPosition {
        NONE_INTERSECTION, AT_START_POINT, AT_MID_POINT, AT_END_POINT, ON_A_WHOLE_LINESEGMENT, NOT_ON_LINESEGMENT_BUT_ON_EXTENDED_LINE};

protected:
	rg_Point2D m_StartPoint;
	rg_Point2D m_EndPoint;

public: 
	// Construction/Destruction
	rg_Line2D();
	rg_Line2D(const rg_Point2D &sPt, const rg_Point2D &ePt);
	rg_Line2D(const rg_Line2D &line);
	~rg_Line2D();            
              
	// Access elements	                       
    inline rg_Point2D getSP() const { return m_StartPoint; };
    inline rg_Point2D getEP() const { return m_EndPoint; };
    inline rg_Line2D  get_reversed_line2D() const { return rg_Line2D(m_EndPoint, m_StartPoint); };

    inline void   setSP(const rg_Point2D &pt) { m_StartPoint = pt; };
    inline void   setEP(const rg_Point2D &pt) { m_EndPoint = pt; };
    inline void   setLine2D(const rg_Point2D &sPt, const rg_Point2D &ePt) { m_StartPoint = sPt;  m_EndPoint = ePt; };
    inline void   setLine2D(const rg_Line2D &line) { m_StartPoint = line.m_StartPoint; m_EndPoint = line.m_EndPoint; };

	// Operations
    inline rg_REAL   getLength() const;
    inline rg_REAL   getSquaredLength() const;
    inline rg_REAL   getDistance(const rg_Point2D& point) const;
    inline rg_REAL	 get_parameter_of_point_on_line_segment(const rg_Point2D& point) const;

    inline rg_REAL   signed_distance(const rg_Point2D& point) const;
    inline bool      does_contain(const rg_Point2D& point, const rg_REAL& res = resNeg6) const;

    inline rg_REAL   signed_distance_as_line_segment(const rg_Point2D& point) const;
    //inline bool      does_contain_in_line_segment(const rg_Point2D& point) const;
    bool      does_contain_in_line_segment( const rg_Point2D& point ) const;

    inline rg_Point2D  evaluateVector()  const;
    inline rg_Point2D  getNormalVector() const;
    inline rg_Point2D  evaluate_point(const double& param) const;

    void rotate_this_line_about_point(const rg_REAL& angle, const rg_Point2D& point);

    inline void get_coefficients_of_implicit_form_of_line_equation(rg_REAL& coefficientOfX, rg_REAL& coefficientOfY, rg_REAL& coefficientOfConstant) const;
    inline void get_coefficients_of_implicit_form_of_line_equation_in_normalized(rg_REAL& coefficientOfXNormalized, rg_REAL& coefficientOfYNormalized, rg_REAL& coefficientOfConstantNormalized) const;

    rg_Point2D  project(const rg_Point2D& givenPoint, rg_REAL& parameterOfFoot) const;
    rg_Point2D  compute_footprint_of_point_onto_line_segment(const rg_Point2D& givenPoint, rg_REAL& parameterOfClosestPoint) const;
    bool        compute_footprint_of_point_onto_line_segment(const rg_Point2D& givenPoint, rg_Point2D& footOnLineSegment) const;

    rg_Point2D  compute_perpendicular_footprint_of_point_onto_entire_line(const rg_Point2D& givenPoint, rg_REAL& parameterOfClosestPoint) const;
    bool        compute_perpendicular_footprint_of_point_onto_entire_line(const rg_Point2D& givenPoint, rg_Point2D& footOnLineSegment) const;
    rg_Point2D  compute_intersection_with_line(const rg_Line2D& line, bool& bTwoLinesAreParallel) const;
    rg_Point2D  compute_intersection_btw_two_line_segments(const rg_Line2D& lineSegment, bool& doesIntersect, IntersectionPointPosition& position) const;
    rg_Point2D  compute_tangent_vector_at_this_point_for_marching_direction(const rg_Point2D& givenPoint, const bool& bLeftFace, const rg_Point2D& marchingPoint);

    inline rg_Line2D make_perpendicular_line(const rg_Point2D& passingPoint);
	inline rg_Line2D make_parallel_line_to_normal_direction(const rg_REAL& distance);
	
    // Operator Overloading
    rg_FLAG     operator==(const rg_Line2D &line) const;
    rg_Line2D&  operator =(const rg_Line2D& line);

    inline bool is_parallel_to(const rg_Line2D& line)       const;
    bool        is_overlapped(const rg_Line2D& lineSegment) const;
    bool        does_intersect_as_line_segment(const rg_Line2D& lineSegment)const;
    bool        Is_perpendicular_footprint_of_point_on_line_segment( const rg_Point2D& givenPoint );

    /**
    * \brief convert a line passing through two points into the implicit form ( \f$Ax + By + C = 0 (A^2 + B^2 = 1)\f$ ).
    */
    bool        convertIntoImplicitForm(double& A, double& B, double& C);
};


inline rg_REAL rg_Line2D::getLength() const
{
	rg_Point2D     length(m_EndPoint - m_StartPoint);
	return length.magnitude();
}


inline rg_REAL rg_Line2D::getSquaredLength() const
{
    rg_Point2D     length(m_EndPoint - m_StartPoint);
    return length.magnitudeSquare();
}


inline rg_REAL rg_Line2D::getDistance(const rg_Point2D& point) const
{

    rg_Point2D   spPtVector( point-m_StartPoint );
    rg_Point2D   epPtVector( point-m_EndPoint );
    rg_Point2D   lineVector( m_EndPoint   -m_StartPoint );

    rg_REAL spPtLen = spPtVector.magnitude();
    rg_REAL epPtLen = epPtVector.magnitude();
    rg_REAL lineLen = lineVector.magnitude();

    if ( rg_GE( epPtLen*epPtLen, (spPtLen*spPtLen)+(lineLen*lineLen)) )
    {
        return spPtLen;
    }
    else if ( rg_GE( (spPtLen*spPtLen), (epPtLen*epPtLen)+(lineLen*lineLen)) )
    {
        return epPtLen;
    }
    else
    {
/*
        rg_Point2D   unitRoofVector = spPtVector.getUnitVector();
        rg_Point2D   unitLineVector = lineVector.getUnitVector();

        rg_REAL roofLength = spPtVector.magnitude();

        rg_REAL cosine = unitRoofVector % unitLineVector;
        rg_REAL sine   = sqrt(1-cosine*cosine);

        return roofLength*sine;
*/
        rg_REAL output= rg_ABS(lineVector*spPtVector)/lineLen;
        return output;
    }
}


inline rg_REAL rg_Line2D::signed_distance(const rg_Point2D& point) const
{
    rg_Point2D   spPtVector(point - m_StartPoint);
    rg_Point2D   lineVector(m_EndPoint - m_StartPoint);

    rg_REAL output = lineVector*spPtVector / lineVector.magnitude();
    return output;
}


inline bool rg_Line2D::does_contain(const rg_Point2D& point, const rg_REAL& res) const
{
    return (rg_ZERO(signed_distance(point), res)) ? true : false;
}


inline rg_Point2D rg_Line2D::evaluateVector() const
{
    rg_Point2D output=m_EndPoint-m_StartPoint;
    return output;
}


inline rg_Point2D rg_Line2D::getNormalVector() const
{
    rg_Point2D dirVec = evaluateVector();
    return rg_Point2D(-dirVec.getY(), dirVec.getX());
}


inline rg_REAL rg_Line2D::get_parameter_of_point_on_line_segment(const rg_Point2D & point) const
{
    double parameter = DBL_MAX;

    if (point == m_StartPoint) {
        parameter = 0.0;
    }
    else if (point == m_EndPoint) {
        parameter = 1.0;
    }
    else {
        if (rg_EQ(m_StartPoint.getX(), m_EndPoint.getX())) {
            parameter = (point.getY() - m_StartPoint.getY()) / (m_EndPoint.getY() - m_StartPoint.getY());
        }
        else if (rg_EQ(m_StartPoint.getY(), m_EndPoint.getY())) {
            parameter = (point.getX() - m_StartPoint.getX()) / (m_EndPoint.getX() - m_StartPoint.getX());

        }
        else {
            double t[2];
            t[0] = (point.getX() - m_StartPoint.getX()) / (m_EndPoint.getX() - m_StartPoint.getX());
            t[1] = (point.getY() - m_StartPoint.getY()) / (m_EndPoint.getY() - m_StartPoint.getY());

            if (rg_EQ(t[0], t[1])) {
                parameter = t[0];
            }
        }
    }

    return parameter;


    /* [CYSONG, 2020.03.31] replaced from "LineSegment2D::evaluate_parameter(const rg_Point2D& point)"

    rg_REAL parameter = 0.0;

    rg_REAL xCoordOfStart = sp.getX();
    rg_REAL yCoordOfStart = sp.getY();
    rg_REAL xCoordOfEnd = ep.getX();
    rg_REAL yCoordOfEnd = ep.getY();
    rg_REAL deltaX = xCoordOfEnd - xCoordOfStart;
    rg_REAL deltaY = yCoordOfEnd - yCoordOfStart;

    if (rg_NZERO(deltaX))
    {
        parameter = (point.getX() - xCoordOfStart) / deltaX;
    }

    if (rg_NZERO(deltaY))
    {
        parameter = (point.getY() - yCoordOfStart) / deltaY;
    }

    return parameter;
    */
}


inline rg_Point2D  rg_Line2D::evaluate_point(const double& param) const
{
    return m_StartPoint + param * (m_EndPoint - m_StartPoint);
}


inline void rg_Line2D::get_coefficients_of_implicit_form_of_line_equation(rg_REAL& coefficientOfX, rg_REAL& coefficientOfY, rg_REAL& coefficientOfConstant) const
{
    rg_REAL x1 = m_StartPoint.getX();
    rg_REAL y1 = m_StartPoint.getY();
    rg_REAL x2 = m_EndPoint.getX();
    rg_REAL y2 = m_EndPoint.getY();

    coefficientOfX = y2 - y1;
    coefficientOfY = x1 - x2;
    coefficientOfConstant = x2 * y1 - x1 * y2;
}


inline void rg_Line2D::get_coefficients_of_implicit_form_of_line_equation_in_normalized(rg_REAL& coefficientOfXNormalized, rg_REAL& coefficientOfYNormalized, rg_REAL& coefficientOfConstantNormalized) const
{
    rg_REAL coefficientOfX = 0.0;
    rg_REAL coefficientOfY = 0.0;
    rg_REAL coefficientOfConstant = 0.0;
    get_coefficients_of_implicit_form_of_line_equation(coefficientOfX, coefficientOfY, coefficientOfConstant);

    rg_REAL norm = sqrt(coefficientOfX*coefficientOfX + coefficientOfY * coefficientOfY);

    coefficientOfXNormalized = coefficientOfX / norm;
    coefficientOfYNormalized = coefficientOfY / norm;
    coefficientOfConstantNormalized = coefficientOfConstant / norm;
}


inline bool rg_Line2D::is_parallel_to(const rg_Line2D& line) const
{
    rg_REAL coeffX1 = 0.0, coeffY1 = 0.0, coeffConst1 = 0.0;
    get_coefficients_of_implicit_form_of_line_equation_in_normalized(coeffX1, coeffY1, coeffConst1);
    rg_REAL coeffX2 = 0.0, coeffY2 = 0.0, coeffConst2 = 0.0;
    line.get_coefficients_of_implicit_form_of_line_equation_in_normalized(coeffX2, coeffY2, coeffConst2);

    rg_REAL determinant = coeffX2 * coeffY1 - coeffX1 * coeffY2;

    // Two lines are parallel.
    if (rg_ZERO(fabs(determinant), rg_MATH_RES))
        return true;
    else
        return false;
}


inline rg_Line2D rg_Line2D::make_perpendicular_line(const rg_Point2D& passingPoint)
{
    rg_Point2D dirVec = getNormalVector();
    return rg_Line2D(passingPoint, passingPoint + dirVec);
}


inline rg_Line2D rg_Line2D::make_parallel_line_to_normal_direction(const rg_REAL& distance)
{
    rg_Point2D dirVec = getNormalVector();
    dirVec = dirVec.getUnitVector();
    return rg_Line2D(m_StartPoint + distance * dirVec, m_EndPoint + distance * dirVec);
}


inline rg_REAL rg_Line2D::signed_distance_as_line_segment(const rg_Point2D& point) const
{
    rg_Point2D   spPtVector(point - m_StartPoint);
    rg_Point2D   epPtVector(point - m_EndPoint);
    rg_Point2D   lineVector(m_EndPoint - m_StartPoint);

    rg_REAL spPtLen = spPtVector.magnitude();
    rg_REAL epPtLen = epPtVector.magnitude();
    rg_REAL lineLen = lineVector.magnitude();

    if (rg_GE(epPtLen*epPtLen, (spPtLen*spPtLen) + (lineLen*lineLen))) {
        return spPtLen;
    }
    else if (rg_GE((spPtLen*spPtLen), (epPtLen*epPtLen) + (lineLen*lineLen))) {
        return epPtLen;
    }
    else{
        return (lineVector*spPtVector/lineLen);
    }
}


#endif

