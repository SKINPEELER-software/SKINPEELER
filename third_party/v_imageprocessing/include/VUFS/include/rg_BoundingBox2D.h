#ifndef _rg_BoundingBox_H
#define _rg_BoundingBox_H

#include "rg_Const.h"
//#include "defconst.h"
#include "rg_Point2D.h"
#include "rg_RelativeOp.h"
#include "rg_dList.h"
#include "rg_Circle2D.h"

#include <list>
using namespace std;

class Ellipse2D;


const rg_Point2D InfinitPt2D(rg_MAX_REAL,rg_MAX_REAL);

class rg_BoundingBox2D
{
private:
    rg_Point2D min;
    rg_Point2D max;

public:
    // construtors and destructor
    rg_BoundingBox2D();
    rg_BoundingBox2D(const rg_Point2D& pt);
    rg_BoundingBox2D(const rg_Point2D& tMinPt, const rg_Point2D& tMaxPt);
	rg_BoundingBox2D(rg_dList<rg_Circle2D>& circles);
    rg_BoundingBox2D(const rg_BoundingBox2D& temp);
    ~rg_BoundingBox2D();

    rg_Point2D getMinPt() const;
    rg_Point2D getMaxPt() const;
    void       setMinPt(const rg_Point2D& tMin);
    void       setMaxPt(const rg_Point2D& tMax);
    rg_Point2D getCenterPt() const;
    
	rg_REAL    evaluateXLength() const;
	rg_REAL    evaluateYLength() const;
    rg_REAL    evaluateLongestLength() const;
    rg_REAL    evaluateAspectRatio() const;

	void       refitAspectRatio(const rg_REAL& ratio);

    void contain(const rg_Point2D& pt);
    void contain(const rg_Point2D*  pts,
                 const rg_INT&     size );
    void contain(const rg_BoundingBox2D& tBox);

	// added by JRYU
	void constructBoxByAddingCircles(rg_dList<rg_Circle2D>& circles);
	void updateBoxByAddingCircle(const rg_Circle2D& circle);
	void updateBoxByAddingEllipse(const Ellipse2D& ellipse);
    void get_boundary_points_in_CCW(list<rg_Point2D>& boundaryPoints);
    void get_boundary_points_in_CW(list<rg_Point2D>& boundaryPoints);

    void reset();
	rg_BoundingBox2D evaluateRelativeOffset(const rg_REAL& ratio) const;
	rg_BoundingBox2D evaluateAbsoluteOffset(const rg_REAL& dist) const;
    // operator overloading
    rg_BoundingBox2D& operator=(const rg_BoundingBox2D& temp);

    // ETC functions
    rg_FLAG isOverlapped(const rg_BoundingBox2D& temp) const;
    rg_FLAG isNull() const;
    bool doesContain(const rg_Point2D& point, const double& res) const; //CYSONG[Oct17, 19]: function added
};
#endif






