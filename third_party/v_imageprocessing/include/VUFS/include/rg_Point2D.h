/////////////////////////////////////////////////////////////////////
//	 
//    FILENAME    : rg_Point2D.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_Point2D 
//           which define point and its property. 
//
//    BASE CLASS  : None  
//
//    AUTHOR      : Deok-Soo Kim, Soon-Woong Lee
//    START DATE  : 8 Sep. 1996    
//
//    HISTORY     :
//
//    1. Lee, Soon-Woong inserted the following function at 21 Jul. 1997.
//                      rg_Point2D getUnitVector()
//    2. Jang, Tae-Bum  inserted the following operator at 1997.7.22.
//                      rg_REAL %(const rg_Point2D& point);// dot product
//	  3. Kim, Donguk    inserted the following function at 1999.9.8.
//						rg_REAL distance(const rg_Point2D& point);
//    
//            Copyright ¨Ï CAD/CAM Lab.    	  	
//
/////////////////////////////////////////////////////////////////////

#ifndef	_RG_POINT2D_H
#define _RG_POINT2D_H

#include "rg_Const.h"
#include "rg_RelativeOp.h"

#include <cmath>
using namespace std;


class rg_Line2D;
class rg_Parabola2D;
class rg_GeoFunc;


/**
 * \brief This is the interface of the class rg_Point2D which define point and its property. 
 * \author Deok-Soo Kim, Soon-Woong Lee 
 * \date Sep08. 1996   
 *
 * HISTORY:
 *    1. Lee, Soon-Woong inserted the following function at 21 Jul. 1997.
 *                      rg_Point2D getUnitVector()
 *    2. Jang, Tae-Bum  inserted the following operator at 1997.7.22.
 *                      rg_REAL %(const rg_Point2D& point);// dot product
 *	  3. Kim, Donguk    inserted the following function at 1999.9.8.
 *						rg_REAL distance(const rg_Point2D& point);
 * 
 */
class rg_Point2D 
{
private:
	rg_REAL x;
	rg_REAL y;

public:
	
    /**
     * \brief Construct and initialize a rg_Point2D object to a point (0.0, 0.0).
     */
    rg_Point2D();
	
    
    /**
     * \brief Construct and initialize a rg_Point2D object from the specified rg_Point2D object.
     *
     * \param point the specified rg_Point2D object
     */
    rg_Point2D(const rg_Point2D &point);
	

    /**
     * \brief Construct and initialize a rg_Point2D object from the specified x and y values.
     *
     * \param px the spedified x value
     * \param py the specified y value
     */
	rg_Point2D(const rg_REAL    &px, const rg_REAL &py);


    /**
     * \brief Destruct the rg_Point2D object.
     */
	~rg_Point2D();



    /**
     * \brief Get the x-coord. value of this object.
     *
     * \return the x-coord. value
     */
    rg_REAL     getX() const;


    /**
     * \brief Get the y-coord. value of this object.
     *
     * \return the y-coord. value
     */
	rg_REAL     getY() const;
	

    /**
     * \brief Set the x-coord. value of this object.
     *
     * \param px the x-coord. value
     */
	void        setX( const rg_REAL& px );


    /**
     * \brief Set the y-coord. value of this object.
     *
     * \param py the y-coord. value
     */
	void        setY( const rg_REAL& py );


    /**
     * \brief Set this object as the specified rg_Point2D object.
     *
     * \param point the specified rg_Point2D object
     */
	void        setPoint(const rg_Point2D& point);


    /**
     * \brief Set this object as the specified x and y values.
     *
     * \param px the x-coord. value
     * \param py the y-coord. value
     */
	void        setPoint(const rg_REAL& px, const rg_REAL& py);



    /**
     * \brief  [vector operation] Calculate magnitude of this vector.
     *
     * \return the magnitude of vector
     */
    rg_REAL     magnitude() const;


    /**
     * \brief [vector operation] Calculate squared magnitude of this vector.
     *
     * \return the squared magnitude of vector
     */
    rg_REAL     magnitudeSquare() const;


    /**
     * \brief [vector operation] Get unit vector of this vector.
     *
     * \return the unit vector
     */
    rg_Point2D  getUnitVector() const;


    /**
    * \brief [vector operation] Get perpendicular vector of this vector.
    *
    * \return the perpendicular vector
    */
    rg_Point2D  getPerpendicularVector() const;


    /**
    * \brief [vector operation] Get CCW perpendicular vector of this vector.
    *
    * \return the CCW perpendicular vector
    */
    rg_Point2D  getPerpendicularVector_CCW() const;


    /**
    * \brief [vector operation] Get CW perpendicular vector of this vector.
    *
    * \return the CW perpendicular vector
    */
    rg_Point2D  getPerpendicularVector_CW() const;


    /**
     * \brief Calculate the Euclidean distance from this object to another rg_Point2D object.
     *
     * \param point another rg_Point2D object
     *
     * \return the Euclidean distance from this object to another
     */
    rg_REAL     distance(const rg_Point2D &point) const;


    /**
     * \brief Calculate the squared Euclidean distance from this object to another rg_Point2D object.
     *
     * \param point another rg_Point2D object
     *
     * \return the squared Euclidean distance from this object to another
     */
    rg_REAL     squaredDist(const rg_Point2D& point) const;
	

    /**
     * \brief INVALID.
     *
     * \param givenPoint INVALID
     * \param givenPoint INVALID
     *
     * \return true always
     */
    bool        compute_perpendicular_footprint_of_point_onto_this_point(const rg_Point2D& givenPoint, rg_Point2D& footOnThisPoint) const;
    
    
    /**
     * \brief [vector operation] Compute tangent vector at given point of a body when marching point is given.
     *
     * The orientation of the tangent vector is decided by bLeftFace.
     * If bLeftFace is true, the output vector is generated by rotating the vector (marchingPoint - givenPoint) CCW.
     * Otherwise, that is generated by rotating the vector (marchingPoint - givenPoint) CW.    
     *
     * \param givenPoint the given point of a body
     * \param bLeftFace the boolean for output tangent vector to touch a body on the left side 
     * \param marchingPoint the marching point
     *
     * \return tangent vector w.r.t. (marchingPoint - givenPoint)
     */    
    static rg_Point2D compute_tangent_vector_at_this_point_for_marching_direction(const rg_Point2D& givenPoint, const bool& bLeftFace, const rg_Point2D& marchingPoint);



    /**
     * \brief Replace the rg_Point2D object with another rg_Point2D obejct, \a point.
     *
     * \param point the rg_Point2D object to be copied
     *
     * \return *this
     */ 
	rg_Point2D&  operator  =( const rg_Point2D &point );


    /**
     * \brief Test if the rg_Point2D object is equal to another one.
     *
     * \param point another vector to be compared
     *
     * \return true(1) if two objects are equal, otherwise false(0)
     */ 
	rg_INT       operator ==( const rg_Point2D &point ) const;


    /**
     * \brief [vector operation] Add this vector to another one.
     *
     * \param point another vector to be added
     *
     * \return the output vector of the addition of two vectors
     */ 
	rg_Point2D   operator  +( const rg_Point2D &point ) const;


    /**
     * \brief [vector operation] Add this vector to another one and store the result to this.
     *
     * \param point another vector to be added
     *
     * \return *this
     */ 
	rg_Point2D&  operator +=( const rg_Point2D &point );


    /**
     * \brief [vector operation] Subtract a vector from this.
     *
     * \param point another vector to subtract
     *
     * \return the output vector of the subtraction of two vectors
     */ 
	rg_Point2D   operator  -( const rg_Point2D &point ) const;


    /**
     * \brief [vector operation] Subtract a vector from this vector and store the result to this.
     *
     * \param point another vector to subtract
     *
     * \return *this
     */ 
	rg_Point2D&  operator -=( const rg_Point2D &point );


    /**
     * \brief [vector operation] Multiply constant to this vector.
     *
     * \param n the constant
     *
     * \return the output vector of the multiplication
     */ 
	rg_Point2D   operator  *( const rg_REAL&     n ) const;


    /**
     * \brief [vector operation] Calculate the cross product from this vector to another.
     *
     * \param point the second operand of cross product
     *
     * \return the magnitude of the cross producted vector
     */ 
	rg_REAL      operator  *( const rg_Point2D &point ) const;	


    /**
     * \brief [vector operation] Divide this vector by constant.
     *
     * \param n the constant
     *
     * \return the output vector of the division
     */ 
	rg_Point2D   operator  /( const rg_REAL&     n ) const;


    /**
     * \brief [vector operation] Set this vector as (n, n).
     *
     * \param n the constant
     *
     * \return *this
     */
	rg_Point2D&  operator  =( const rg_REAL&     n );


    /**
     * \brief [vector operation] Calculate the dot product of this vector and another.
     *
     * \param point the second operand of dot product
     *
     * \return the dot product of this vector and another
     */
    rg_REAL      operator  %( const rg_Point2D &point) const;	
	

    /**
     * \brief [vector operation] Multiply constant to an input vector.
     *
     * \param n the constant
     * \param point a vector
     *
     * \return the output vector of the multiplication
     */ 
	friend rg_Point2D operator *( const rg_REAL n, const rg_Point2D& point );


    /**
     * \brief [vector operation] Replace the sign of an input vector.
     *
     * \param point a vector
     *
     * \return -point
     */ 
	friend rg_Point2D operator -( const rg_Point2D &point );


    /**
     * \brief [vector operation] Calculate the angle from vector1 to vector2 in CCW.
     *
     * [History] 
     * - Redefined as inline function (June 29, 2017 by Joonghyun)
     *
     * \param vector1 the first vector
     * \param vector2 the second vector
     *
     * \return angle in radian
     */ 
    friend rg_REAL angleFromVec1toVec2(const rg_Point2D& vector1, const rg_Point2D& vector2);


    /**
     * \brief Test if two points are equal within tolerance.
     *
     * \param pt the rg_Point2D object to be compared
     * \param tolerance the tolerance (if not set, the default value is 10-6)
     *
     * \return true if two points are equal, otherwise false
     */ 
    rg_BOOL     isEqual(const rg_Point2D &pt, const rg_REAL& tolerance = resNeg6) const;


    /**
     * \brief Calculate the bisector of two points (A: this, B: input).
     *
     * \param pt the rg_Point2D object (B)
     * \param bisector the output bisector of two points A, B
     * \param dist the distance from one end point of the bisector to the line segment AB (if not set, the default value is 5.0)
     *
     * \return true if two points are not equal, otherwise false
     */ 
    bool        calculateBisector(const rg_Point2D &pt, rg_Line2D& bisector, const rg_REAL& dist = 5.0) const;


    /**
     * \brief Calculate the bisector of this point and a line.
     *
     * \param line the rg_Line2D object
     * \param bisector the output parabola bisector of this point and a line
     * \param dist INVALID 
     *
     * \return true if this point is not one the line, otherwise false
     */ 
    bool        calculateBisector(const rg_Line2D &line, rg_Parabola2D& bisector, const rg_REAL& dist = 5.0) const;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// inline functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline  rg_REAL rg_Point2D::getX() const    { return x; }
inline  rg_REAL rg_Point2D::getY() const    { return y; }

inline  void rg_Point2D::setX(const rg_REAL &px)    { x = px; }

inline  void rg_Point2D::setY(const rg_REAL &py)    { y = py; }

inline  void rg_Point2D::setPoint(const rg_Point2D &point) {
    x = point.x;
    y = point.y;
}

inline  void rg_Point2D::setPoint(const rg_REAL &px, const rg_REAL &py) {
    x = px;
    y = py;
}

inline rg_REAL rg_Point2D::magnitude() const        { return sqrt(x*x + y*y); }

inline rg_REAL rg_Point2D::magnitudeSquare() const  { return (x*x + y*y); }

inline rg_Point2D rg_Point2D::getUnitVector() const { return *this / magnitude(); }

inline rg_Point2D rg_Point2D::getPerpendicularVector() const { return getPerpendicularVector_CCW(); }

inline rg_Point2D rg_Point2D::getPerpendicularVector_CCW() const { return rg_Point2D( -y, x ); }

inline rg_Point2D rg_Point2D::getPerpendicularVector_CW() const { return rg_Point2D( y, -x ); }

inline rg_REAL rg_Point2D::distance(const rg_Point2D &point) const {
    return sqrt((x - point.x)*(x - point.x) + (y - point.y)*(y - point.y));
}

inline rg_REAL rg_Point2D::squaredDist(const rg_Point2D& point) const {
    return (x - point.x)*(x - point.x) + (y - point.y)*(y - point.y);
}

inline  bool  rg_Point2D::compute_perpendicular_footprint_of_point_onto_this_point(const rg_Point2D& givenPoint, rg_Point2D& footOnThisPoint) const
{
    footOnThisPoint = *this;
    return true;
}


inline  rg_Point2D  rg_Point2D::compute_tangent_vector_at_this_point_for_marching_direction(const rg_Point2D& givenPoint, const bool& bLeftFace, const rg_Point2D& marchingPoint)
{
    rg_Point2D guideVec = marchingPoint - givenPoint;
    rg_Point2D tangentVectorOnPoint;

    (bLeftFace) ? tangentVectorOnPoint.setPoint(-guideVec.getY(), guideVec.getX()) : tangentVectorOnPoint.setPoint(guideVec.getY(), -guideVec.getX());
    //  modified by YCho 2017.09.23 
    //
    //switch (bLeftFace)  {
    //    case true:
    //    {
    //        // cross product cannot be zero.
    //        tangentVectorOnPoint.setPoint(-guideVec.getY(), guideVec.getX());
    //    }
    //    break;
    //    case false:
    //    {
    //        tangentVectorOnPoint.setPoint(guideVec.getY(), -guideVec.getX());
    //    }
    //    break;
    //}

    return tangentVectorOnPoint;
}


inline rg_BOOL rg_Point2D::isEqual(const rg_Point2D &pt, const rg_REAL& tolerance) const
{
    if ( rg_EQ(x, pt.x, tolerance) && rg_EQ(y, pt.y, tolerance) ) {
        return rg_TRUE;
    }
    else {
        return rg_FALSE;
    }
}


#endif  //rg_Point2D class


