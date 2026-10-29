/////////////////////////////////////////////////////////////////////
//
//	  FILENAME    : rg_Circle2D.h
//	  
//    DESCRIPTION : 
//           This is the interface of the class rg_Circle2D 
//           which define circle and its property. 
//
//    BASE CLASS  : None  
//
//    AUTHOR      : Donguk Kim
//    START DATE  : 1999.  9.  8. 
//
//    HISTORY     :
//
//    
//            Copyright �� CAD/CAM Lab.    	  	
//
/////////////////////////////////////////////////////////////////////

#ifndef _RG_CIRCLE2D_H
#define _RG_CIRCLE2D_H

#include "rg_Const.h"
#include "rg_Point2D.h"
#include "rg_Line2D.h"
#include "rg_ImplicitEquation.h"




/**
 * \brief This is the interface of the class rg_Circle2D which define circle and its property. 
 * \author Donguk Kim
 * \date Sep08. 1999   
 */
class rg_Circle2D
{
private:
	rg_Point2D centerPt;
	rg_REAL	   radius;
	
public:

    /**
     * \brief Construct and initialize a rg_Circle2D object with center point (0.0, 0.0), radius 0.0.
     */
	rg_Circle2D();


    /**
     * \brief Construct and initialize a rg_Circle2D object by three passing points.
     *
     * HISTORY:
     *    Ryu, Joonghyun created this function on December 2, 2016.
     */
	rg_Circle2D(const rg_Point2D& point0, const rg_Point2D& point1, const rg_Point2D& point2);
	
	
    /**
     * \brief Construct and initialize a rg_Circle2D object by the specified center point and radius.
     */	
	rg_Circle2D(const rg_Point2D& center, const rg_REAL& r);


    /**
     * \brief Construct and initialize a rg_Circle2D object by the specified center point x, y and radius.
     */	
	rg_Circle2D(const rg_REAL& pointX, const rg_REAL& pointY, const rg_REAL& r);


    /**
     * \brief Construct and initialize a rg_Circle2D object from the specified rg_Circle2D object.
     *
     * \param circle the specified rg_Circle2D object
     */
	rg_Circle2D(const rg_Circle2D& circle);
	

    /**
     * \brief Destruct the rg_Circle2D object.
     */
	virtual ~rg_Circle2D();
	

    /**
     * \brief Get the center point of this circle object.
     *
     * \return the center point
     */
	inline rg_Point2D  getCenterPt() const { return centerPt; }
    inline rg_REAL  getX() const { return centerPt.getX(); }
    inline rg_REAL  getY() const { return centerPt.getY(); }
    /**
     * \brief Get the radius of this circle object.
     *
     * \return the radius
     */
	inline rg_REAL     getRadius()   const { return radius; }


    /**
     * \brief Get this circle object.
     *
     * \return *this (Note: this fucntion returns copied object)
     */
	inline rg_Circle2D getCircle()   const { return *this; } 
  

    /**
     * \brief Calculate the area of this circle object.
     *
     * HISTORY:
     *  Song, Chanyoung added const to clarify no member data is changed on Sep 02, 2019.  
     *
     * \return the area of this circle
     */
	inline rg_REAL computeArea() const { return rg_PI * radius * radius; }


    /**
     * \brief Calculate the perimeter of this circle object.
     *
     * HISTORY:
     *  Song, Chanyoung added const to clarify no member data is changed on Sep 02, 2019.  
     *
     * \return the perimeter of this circle
     */
	inline rg_REAL computePerimeter() const { return 2. * rg_PI * radius; }
	

    /**
     * \brief Set the center point of this circle object.
     *
     * \param center the center point
     */
	void setCenterPt(const rg_Point2D& center);
    inline  void setX(const rg_REAL &px)    { centerPt.setX(px); }
    inline  void setY(const rg_REAL &py)    { centerPt.setY(py); }

    /**
     * \brief Set the radius of this circle object.
     *
     * \param r the radius
     */	
    void setRadius(const rg_REAL& r);


    /**
     * \brief Set the circle by the specified rg_Circle2D object.
     *
     * \param circle the specified rg_Circle2D object
     */	
	void setCircle(const rg_Circle2D& circle);


    /**
     * \brief Set the circle by the specified center point and radius.
     *
     * \param center the specified center point 
     * \param r the specified radius
     */	
	void setCircle(const rg_Point2D& center, const rg_REAL& r);


    /**
     * \brief Set the circle by three passing points in CCW order.
     *
     * HISTORY:
     *   Ryu, Joonghyun created this function on December 3, 2016. 
     *
     * \param point0_CCW the first specified point in CCW order 
     * \param point1_CCW the second specified point in CCW order 
     * \param point2_CCW the third specified point in CCW order 
     */	
    void setCircleWithCCWThreePassingPoints(const rg_Point2D& point0_CCW, const rg_Point2D& point1_CCW, const rg_Point2D& point2_CCW); 
   
    

    /**
     * \brief Replace the rg_Circle2D object with another rg_Circle2D obejct, \a circle.
     *
     * \param circle the rg_Circle2D object to be copied
     *
     * \return *this
     */ 
	rg_Circle2D& operator=(const rg_Circle2D& circle);


    /**
     * \brief Test if the rg_Circle2D object is equal to another one.
     *
     * \param circle another circle to be compared
     *
     * \return true if two objects are equal, otherwise false
     */ 
    rg_BOOL operator==(const rg_Circle2D& circle) const;



    /**
     * \brief Set the circle by three passing points.
     *
     * HISTORY:
     *   Ryu, Joonghyun created this function on December 10, 2016. 
     *
     * \param point0 the first specified point  
     * \param point1 the second specified point 
     * \param point2 the third specified point 
     */	
    void setCircleWithThreePassingPoints(const rg_Point2D& point1, const rg_Point2D& point2, const rg_Point2D& point3); 


    /**
     * \brief Get two intesection points between this circle and another one.
     *
     * The output two points uses dynamically allocated memory if they exist.
     *
     * \param circle another circle
     *
     * \return Always two points if the boundaries of the two disks are intersected or touched at one point,  
     *          otherwise NULL.
     */	
	rg_Point2D* getIntersectPt(const rg_Circle2D& circle) const;	

    
    /**
     * \brief Test whether this circle and another one intersect each other.
     *
     * \param circle another circle
     * \param tolerance tolerance to determine intersection
     *
     * \return true if the two disks are determined to be intersected each other
     *         i.e. dist(c1, c2) < r1 + r2 + tolerance where ci: center, ri:radius of circle i,
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	   
    rg_FLAG     isIntersectWith(const rg_Circle2D& circle, const rg_REAL& tolerance = rg_MATH_RES) const;
	

    /**
     * \brief Test whether this circle and another one intersect, but not tangent to each other.
     *
     * HISTORY:
     *   Ryu, Joonghyun created this function on September 13, 2016.
     *
     * \param circle another circle
     * \param tolerance to determine intersection but not tangent condition
     *
     * \return true if the two disks are determined to be intersected but not tangent each other
     *         i.e. dist(c1, c2) <= r1 + r2 - tolerance where ci: center, ri:radius of circle i,
     *         otherwise, false.
     *        (if not set, the default value is rg_MATH_RES = 10^-6)
     */	        
	rg_FLAG     IntersectWith_notTangentTo(const rg_Circle2D& circle, const rg_REAL& tolerance = rg_MATH_RES) const;
	
    
    /**
     * \brief Test whether this circle are tangent to another inside or outside.
     *
     * \param circle another circle
     * \param tolerance to determine tangent condition
     *
     * \return true if the two disks are determined to be tangent 
     *         i.e. r1 + r2 - tolerance <= dist(c1, c2) <= r1 + r2 + tolerance 
     *              or abs(r1 - r2) - tolerance <= dist(c1, c2) <= abs(r1 - r2) + tolerance     
     *              where ci: center, ri:radius of circle i,
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	 
    rg_FLAG     isTangentTo(const rg_Circle2D& circle, const rg_REAL& tolerance = rg_MATH_RES) const;
	

     /**
     * \brief Test whether this circle is included in another one.
     *
     * \param circle another circle
     * \param tolerance to determine inclusion
     *
     * \return true if this circle is determined to be included in another  
     *         i.e. dist(c1, c2) < r2 - r1 - tolerance     
     *              where ci: each centers; r1: this circle radius, r2: another radius,
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	   
    rg_BOOL     isIncludedIn(const rg_Circle2D& circle, const rg_REAL& tolerance = rg_MATH_RES) const;


    /**
     * \brief Test whether this circle is intersected by the specified line segment.
     *
     * \param lineSegment the specified line segment
     * \param tolerance to determine intersection
     *
     * \return true if this circle is determined to be intersected by the specified line segment  
     *         i.e. min. dist(c, line segment) < r - tolerance,
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	  
    inline bool hasIntersectionWith(const rg_Line2D& lineSegment, const rg_REAL& tolerance = rg_MATH_RES) const;


    /**
     * \brief Test whether the specified point is included in this circle.
     *
     * \param point the specified point
     * \param tolerance to determine inclusion
     *
     * \return true if this circle is determined to include the specified point
     *         i.e. dist(p, c) < r - tolerance,
     *         where p: the specified point, c, r: center, radius of circle, resp.
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	
    rg_BOOL     doesContain(const rg_Point2D& point, const rg_REAL& tolerance = rg_MATH_RES) const;


    /**
     * \brief Test whether the specified circle is included in this circle.
     *
     * \param circle the specified circle
     * \param tolerance to determine inclusion
     *
     * \return true if this circle is determined to include the specified circle
     *         i.e. dist(c1, c2) <= r1 - r2 + tolerance 
     *         where ci: center, ri:radius of circle i,
     *         otherwise, false.
     *         (if not set, the default value is rg_MATH_RES = 10^-6)
     */	
    rg_BOOL     contain(const rg_Circle2D& circle, const rg_REAL& tolerance = rg_MATH_RES) const;


    /**
     * \brief Calculate the ratio of overlapped length to smaller radius if intersected.
     *
     * \param circle the specified circle
     *
     * \return the ratio in [0.0, 1.0]
     */	
    rg_REAL     ratioOfOverlappedCircle(const rg_Circle2D& circle) const;


    /**
     * \brief Calculate the distance from this circle boundary to the specified point.
     *
     * \param point the specified point
     *
     * \return positive value if point is outside of this circle, 
     *         negative value if point is inside of this circle,
     *         zero if point is on the circle boundary.
     */	
    rg_REAL     distance(const rg_Point2D& point) const;


    /**
     * \brief Calculate the distance from this circle boundary to another boundary.
     *
     * \param circle the specified circle
     *
     * \return abs(dist(c1, c2) - (r1 + r2))
     */	
    rg_REAL     distance(const rg_Circle2D& circle) const;


    /**
     * \brief Compute minimum tangent circle both to this circle and the specified circle.
     *
     * \param circle the specified circle
     *
     * \return the tangent circle 
     *         (if not intersected, then positive radius, center point outside of two circles, 
     *          if intersected but not included each other, negative radius, center point inside of two circles,
     *          if included each other, cannot guarantee that the result is same as your expectation.)
     */
    rg_Circle2D computeMinTangentCircle(const rg_Circle2D& circle);


    /**
     * \brief Compute minimum tangent circle both to the two specified points.
     *
     * \param point1 the first specified point
     * \param point2 the second specified point
     *
     * \return the tangent circle 
     *         (if the two points are identical, 
     *          the center point of the result circle is identical to the points with zero radius.)
     */
    friend rg_Circle2D computeMinTangentCircle(const rg_Point2D& point1, const rg_Point2D& point2);


    /**
     * \brief Compute circles tangent from outside to the three specified circles.
     *
     * \param circle1 the first specified circle
     * \param circle2 the second specified circle
     * \param circle3 the third specified circle
     * \param tangentCircle the maximum 2 tangent circles (You should insert array or pointer for storing two circles)
     *
     * \return the number of tangent circles (0~2)       
     */
    friend rg_INT computeCircleTangentTo3CirclesOutside( const rg_Circle2D& circle1, 
                                                         const rg_Circle2D& circle2, 
                                                         const rg_Circle2D& circle3, 
                                                         rg_Circle2D* tangentCircle);
 

    /**
     * \brief Compute circles tangent from outside to the two specified circles given output circle radius. 
     *
     * \param circle1 the first specified circle
     * \param circle2 the second specified circle
     * \param givenRadius the output circle radius
     * \param tangentCircle the maximum 2 tangent circles (You should insert array or pointer for storing two circles)
     *
     * \return the number of tangent circles (0~2)       
     */
    friend rg_INT computeCircleTangentTo2CirclesGivenRadius(
                                                         const rg_Circle2D& circle1, 
                                                         const rg_Circle2D& circle2, 
                                                         const rg_REAL&     givenRadius,
                                                         rg_Circle2D* tangentCircle );


    /**
     * \brief Compute circles tangent from inside to the specified container 
     *         and from outside to the specified circle given output circle radius. 
     *
     *	This function will be merged to the function "computeCircleTangentTo2CirclesGivenRadius()" shortly.
     *
     * \param circle the specified circle
     * \param circularContainer the specified circle container
     * \param givenRadius the output circle radius
     * \param tangentCircle the maximum 2 tangent circles (You should insert array or pointer for storing two circles)
     *
     * \return the number of tangent circles (0~2)       
     */
	friend rg_INT computeCircleTangentToOneCircleAndItsCircularContainerGivenRadius( const rg_Circle2D& circle,
		                                                                             const rg_Circle2D& circularContainer,
		                                                                             const rg_REAL&     givenRadius,
		                                                                             rg_Circle2D* tangentCircle);

    /**
     * \brief Compute circles tangent from outside to the three specified circles.
     *
     * It uses Moibius transformation for calculation.
     *
     * HISTORY
     *  Song, Chanyoung commeted out the same function and added default tolerance value(resNeg15) on April 04, 2020.
     * 
     * \param circle1 the first specified circle
     * \param circle2 the second specified circle
     * \param circle3 the third specified circle
     * \param result1 the output circumcircle 1 
     * \param result2 the output circumcircle 2
     * \param tolerance the tolerance to check the number of circumcircles 
     *
     * \return the number of tangent circles (0~2)       
     */
	static rg_INT makeCircumcircle(const rg_Circle2D& circle1,
		                           const rg_Circle2D& circle2,
		                           const rg_Circle2D& circle3,
		                           rg_Circle2D& result1, rg_Circle2D& result2,
		                           const rg_REAL& tolerance = resNeg15);
            /*  
                Song, Chanyoung commeted out this function on April 04, 2020.

                static rg_INT makeCircumcircle( const rg_Circle2D& circle1, 
                                                const rg_Circle2D& circle2, 
                                                const rg_Circle2D& circle3,
						                        rg_Circle2D& result1, rg_Circle2D& result2);
            */


    /**
     * \brief Compute circles tangent from inside to the specified enclosing circle 
     *         and from outside to the two specified circle. 
     *
     * Enclosing circle and two contained circles always have two tangent circles.
     * 
     * \param enclosingCircle the specified enclosing circle
     * \param circle1 the first specified circle in \a enclosingCircle
     * \param circle2 the second specified circle in \a enclosingCircle
     * \param result1 the output tangent circle 1 
     * \param result2 the output tangent circle 2
     */	
    static void calculateTangentCircles(const rg_Circle2D& enclosingCircle,
										const rg_Circle2D& circle1,
										const rg_Circle2D& circle2,
										rg_Circle2D& result1,
										rg_Circle2D& result2);


    /**
     * \brief Compute one circle tangent from inside to the specified enclosing circle 
     *         and from outside to the two specified circle. [type1]
     *
     * Used in calculateTangentCircles function.
     * 
     * \param x1 the x-coord. of larger circle between circle1 and circle2
     * \param r1 the radius of larger circle between circle1 and circle2
     * \param x2 the x-coord. of enclosingCircle
     * \param y2 the y-coord. of enclosingCircle 
     * \param r2 the radius of enclosingCircle
     *
     * \return the tangent circle             
     */	
	static rg_Circle2D solution1(const double& x1, const double& r1, const double& x2, const double& y2, const double& r2);
	
    
    /**
     * \brief Compute one circle tangent from inside to the specified enclosing circle 
     *         and from outside to the two specified circle. [type2]
     *
     * Used in calculateTangentCircles function.
     * 
     * \param x1 the x-coord. of larger circle between circle1 and circle2
     * \param r1 the radius of larger circle between circle1 and circle2
     * \param x2 the x-coord. of enclosingCircle
     * \param y2 the y-coord. of enclosingCircle 
     * \param r2 the radius of enclosingCircle
     *
     * \return the tangent circle       
     */	   
    static rg_Circle2D solution2(const double& x1, const double& r1, const double& x2, const double& y2, const double& r2);


    /**
     * \brief Test if the rg_Circle2D object is equal to another one.
     *
     * HISTORY
     *   Lee, Mokwon created this function on April 3, 2015.
     *
     * \param circle another circle to be compared
     *
     * \return true if two objects are equal, otherwise false
     */    
    rg_BOOL      isEqual( const rg_Circle2D& circle ) const;


    /**
     * \brief Compute a perpendicular footprint of point on this circle.
     *
     * \param givenPoint 
     * \param footOnCircle output 
     *        (Nearest one between two intersection points of this circle and the line connecting the center point and \a givenPoint)
     *
     * \return true 
     */   
    bool compute_perpendicular_footprint_of_point_onto_circle(const rg_Point2D& givenPoint, rg_Point2D& footOnCircle) const;


    /**
     * \brief Compute the implicit equations of two exterior lines tangent to two circles.
     *
     * \param circle1 the specified first circle
     * \param circle2 the specified second circle
     * \param result1 the output implicit equation 1
     * \param result2 the output implicit equation 2
     */   
    static void makeExteriorTangentLinesOf2Circles( const rg_Circle2D&	circle1,
											        const rg_Circle2D&	circle2,
											        rg_ImplicitEquation&	result1,
											        rg_ImplicitEquation&	result2);


    /**
     * \brief Compute the implicit equations of two interior lines tangent to two circles.
     *
     * \param circle1 the specified first circle
     * \param circle2 the specified second circle
     * \param result1 the output implicit equation 1
     * \param result2 the output implicit equation 2
     */   
    static void makeInteriorTangentLinesOf2Circles(  const rg_Circle2D& circle1,
											         const rg_Circle2D& circle2,
											         rg_ImplicitEquation& result1,
											         rg_ImplicitEquation& result2);

private:

    static void shrinkCircle( const         rg_Circle2D& c1,
                              const         rg_Circle2D& c2,
                              const         rg_Circle2D& c3,
                              rg_Circle2D&  c1Tilde,
                              rg_Circle2D&  c2Tilde,
                              rg_Point2D&   smallest);
    //static void makeExteriorTangentLinesOf2Circles(  const rg_Circle2D&	circle1,
				//							         const rg_Circle2D&	circle2,
				//							         rg_ImplicitEquation&	result1,
				//							         rg_ImplicitEquation&	result2);
    static rg_Point2D makeExteriorTangentLinesInWPlane(  const rg_Circle2D&		c1, 
										                 const rg_Circle2D&		c2,
											             const rg_Circle2D&		c3,
											             rg_ImplicitEquation&	result1,
											             rg_ImplicitEquation&	result2);

    static rg_Circle2D transformZ2W(const rg_Circle2D&	cTilde, const rg_Point2D& smallest);
    static rg_Circle2D transformW2Z(const rg_ImplicitEquation& line, const rg_Point2D& z3);


};

inline bool rg_Circle2D::hasIntersectionWith(const rg_Line2D& lineSegment, const rg_REAL& tolerance /*= rg_MATH_RES*/) const {
    return ( rg_LT(lineSegment.getDistance(centerPt), radius, tolerance) == rg_TRUE ) ? true : false;
}

#endif


