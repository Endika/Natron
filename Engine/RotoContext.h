//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#ifndef KNOBROTO_H
#define KNOBROTO_H

#include <list>
#include <set>
#include <string>

#ifndef Q_MOC_RUN
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/serialization/split_member.hpp>
#endif

#include "Global/GlobalDefines.h"
CLANG_DIAG_OFF(deprecated-declarations)
#include <QObject>
#include <QMutex>
#include <QMetaType>
CLANG_DIAG_ON(deprecated-declarations)

#include "Engine/Rect.h"


#define kRotoLayerBaseName "Layer"
#define kRotoBezierBaseName "Bezier"
#define kRotoBSplineBaseName "B-Spline"
#define kRotoEllipseBaseName "Ellipse"
#define kRotoRectangleBaseName "Rectangle"

namespace Natron {
class Image;
class Node;
}
namespace boost {
namespace serialization {
class access;
}
}

class RectI;
class RectD;
class KnobI;
class Bool_Knob;
class Double_Knob;
class Int_Knob;
class Choice_Knob;
class Color_Knob;

class Curve;
class Bezier;
class RotoItemSerialization;
class BezierSerialization;


/**
 * @class A Bezier is an animated control point of a Bezier. It is the starting point
 * and/or the ending point of a bezier segment. (It would correspond to P0/P3).
 * The left bezier point/right bezier point we refer to in the functions below
 * are respectively the P2 and P1 point.
 *
 * Note on multi-thread:
 * All getters or const functions can be called in any thread, that is:
 * - The GUI thread (main-thread)
 * - The render thread 
 * - The serialization thread (when saving)
 *
 * Setters or non-const functions can exclusively be called in the main-thread (Gui thread) to ensure there is no
 * race condition whatsoever.

 * More-over the setters must be called ONLY by the Bezier class which is the class handling the thread safety.
 * That's why non-const functions are private.
 **/
struct BezierCPPrivate;
class BezierCP
{
    ///This is the unique class allowed to call the setters.
    friend class Bezier;
    friend class boost::serialization::access;

public:

    ///Constructore used by the serialization
    BezierCP();

    BezierCP(const BezierCP & other);

    BezierCP(const boost::shared_ptr<Bezier>& curve);

    virtual ~BezierCP();
    
    boost::shared_ptr<Curve> getXCurve() const;
    boost::shared_ptr<Curve> getYCurve() const;
    boost::shared_ptr<Curve> getLeftXCurve() const;
    boost::shared_ptr<Curve> getLeftYCurve() const;
    boost::shared_ptr<Curve> getRightXCurve() const;
    boost::shared_ptr<Curve> getRightYCurve() const;

    void clone(const BezierCP & other);

    void setPositionAtTime(int time,double x,double y);

    void setLeftBezierPointAtTime(int time,double x,double y);

    void setRightBezierPointAtTime(int time,double x,double y);

    void setStaticPosition(double x,double y);

    void setLeftBezierStaticPosition(double x,double y);

    void setRightBezierStaticPosition(double x,double y);

    void removeKeyframe(int time);

    ///returns true if a keyframe was set
    bool cuspPoint(int time,bool autoKeying,bool rippleEdit);

    ///returns true if a keyframe was set
    bool smoothPoint(int time,bool autoKeying,bool rippleEdit);


    virtual bool isFeatherPoint() const
    {
        return false;
    }

    bool equalsAtTime(int time,const BezierCP & other) const;

    bool getPositionAtTime(int time,double* x,double* y,bool skipMasterOrRelative = false) const;

    bool getLeftBezierPointAtTime(int time,double* x,double* y,bool skipMasterOrRelative = false) const;

    bool getRightBezierPointAtTime(int time,double *x,double *y,bool skipMasterOrRelative = false) const;

    bool hasKeyFrameAtTime(int time) const;

    void getKeyframeTimes(std::set<int>* times) const;

    int getKeyframeTime(int index) const;

    int getKeyframesCount() const;

    int getControlPointsCount() const;

    /**
     * @brief Pointer to the bezier holding this control point. This is not protected by a mutex
     * since it never changes.
     **/
    boost::shared_ptr<Bezier> getBezier() const;

    /**
     * @brief Returns whether a tangent handle is nearby the given coordinates.
     * That is, this function a number indicating
     * if the given coordinates are close to the left control point (P2, ret = 0) or the right control point(P1, ret = 1).
     * If it is not close to any tangent, -1 is returned.
     * This function can also return the tangent of a feather point, to find out if the point is a feather point call
     * isFeatherPoint() on the returned control point.
     **/
    int isNearbyTangent(int time,double x,double y,double acceptance) const;

    /**
     * The functions below are to slave/unslave a control point to a track
     **/
    void slaveTo(SequenceTime offsetTime,const boost::shared_ptr<Double_Knob> & track);
    boost::shared_ptr<Double_Knob> isSlaved() const;
    void unslave();

    SequenceTime getOffsetTime() const;

private:

    template<class Archive>
    void save(Archive & ar, const unsigned int version) const;

    template<class Archive>
    void load(Archive & ar, const unsigned int version);
    
    BOOST_SERIALIZATION_SPLIT_MEMBER()
    
    boost::scoped_ptr<BezierCPPrivate> _imp;
};

class FeatherPoint
    : public BezierCP
{
public:

    FeatherPoint(const boost::shared_ptr<Bezier>& curve)
        : BezierCP(curve)
    {
    }

    virtual bool isFeatherPoint() const
    {
        return true;
    }
};


/**
 * @class A base class for all items made by the roto context
 **/
class RotoContext;
class RotoLayer;

namespace Transform {
struct Matrix3x3;
}

struct RotoItemPrivate;
class RotoItem
: public QObject, public boost::enable_shared_from_this<RotoItem>
{
public:

    RotoItem(const boost::shared_ptr<RotoContext>& context,
             const std::string & name,
             boost::shared_ptr<RotoLayer> parent = boost::shared_ptr<RotoLayer>());

    virtual ~RotoItem();

    virtual void clone(const RotoItem*  other);

    ///only callable on the main-thread
    void setName(const std::string & name);

    std::string getName_mt_safe() const;

    ///only callable on the main-thread
    void setParentLayer(boost::shared_ptr<RotoLayer> layer);

    ///MT-safe
    boost::shared_ptr<RotoLayer> getParentLayer() const;

    ///only callable from the main-thread
    void setGloballyActivated(bool a, bool setChildren);

    ///MT-safe
    bool isGloballyActivated() const;

    bool isDeactivatedRecursive() const;

    void setLocked(bool l,bool lockChildren);
    bool getLocked() const;

    bool isLockedRecursive() const;

    /**
     * @brief Returns at which hierarchy level the item is.
     * The base layer is 0.
     * All items into that base layer are on level 1.
     * etc...
     **/
    int getHierarchyLevel() const;

    /**
     * @brief Must be implemented by the derived class to save the state into
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void save(RotoItemSerialization* obj) const;

    /**
     * @brief Must be implemented by the derived class to load the state from
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void load(const RotoItemSerialization & obj);

    /**
     * @brief Returns the name of the node holding this item
     **/
    std::string getRotoNodeName() const;

    boost::shared_ptr<RotoContext> getContext() const;

protected:


    ///This mutex protects every-member this class and the derived class might have.
    ///That is for the RotoItem class:
    ///  - name
    ///  - globallyActivated
    ///  - locked
    ///  - parentLayer
    ///
    ///For the RotoDrawableItem:
    ///  - overlayColor
    ///
    ///For the RotoLayer class:
    ///  - items
    ///
    ///For the Bezier class:
    ///  - points
    ///  - featherPoints
    ///  - finished
    ///  - pointsAtDistance
    ///  - featherPointsAtDistance
    ///  - featherPointsAtDistanceVal
    mutable QMutex itemMutex;

private:

    void setGloballyActivated_recursive(bool a);
    void setLocked_recursive(bool locked);

    boost::scoped_ptr<RotoItemPrivate> _imp;
};

Q_DECLARE_METATYPE(RotoItem*);


/**
 * @brief Base class for all drawable items
 **/
struct RotoDrawableItemPrivate;
class RotoDrawableItem
    : public RotoItem
{
    Q_OBJECT

public:


    RotoDrawableItem(const boost::shared_ptr<RotoContext>& context,
                     const std::string & name,
                     const boost::shared_ptr<RotoLayer>& parent);

    virtual ~RotoDrawableItem();

    virtual void clone(const RotoItem*  other);

    /**
     * @brief Must be implemented by the derived class to save the state into
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void save(RotoItemSerialization* obj) const OVERRIDE;

    /**
     * @brief Must be implemented by the derived class to load the state from
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void load(const RotoItemSerialization & obj) OVERRIDE;

    /**
     * @brief When deactivated the spline will not be taken into account when rendering, neither will it be visible on the viewer.
     * If isGloballyActivated() returns false, this function will return false aswell.
     **/
    bool isActivated(int time) const;

    /**
     * @brief The opacity of the curve
     **/
    double getOpacity(int time) const;

    /**
     * @brief The distance of the feather is the distance from the control point to the feather point plus
     * the feather distance returned by this function.
     **/
    double getFeatherDistance(int time) const;

    /**
     * @brief The fall-off rate: 0.5 means half color is faded at half distance.
     **/
    double getFeatherFallOff(int time) const;


    /**
     * @brief The color that the GUI should use to draw the overlay of the shape
     **/
    void getOverlayColor(double* color) const;
    void setOverlayColor(const double* color);

    bool getInverted(int time) const;

    void getColor(int time,double* color) const;

    int getCompositingOperator(int time) const;

    std::string getCompositingOperatorToolTip() const;
    boost::shared_ptr<Bool_Knob> getActivatedKnob() const;
    boost::shared_ptr<Double_Knob> getFeatherKnob() const;
    boost::shared_ptr<Double_Knob> getFeatherFallOffKnob() const;
    boost::shared_ptr<Double_Knob> getOpacityKnob() const;
#ifdef NATRON_ROTO_INVERTIBLE
    boost::shared_ptr<Bool_Knob> getInvertedKnob() const;
#endif
    boost::shared_ptr<Choice_Knob> getOperatorKnob() const;
    boost::shared_ptr<Color_Knob> getColorKnob() const;

    const std::list<boost::shared_ptr<KnobI> >& getKnobs() const;
    
signals:

#ifdef NATRON_ROTO_INVERTIBLE
    void invertedStateChanged();
#endif

    void overlayColorChanged();

    void shapeColorChanged();

    void compositingOperatorChanged(int,int);

private:

    boost::scoped_ptr<RotoDrawableItemPrivate> _imp;
};

/**
 * @class A RotoLayer is a group of RotoItem. This allows the context to sort
 * and build hierarchies of layers.
 **/
struct RotoLayerPrivate;
class RotoLayer
    : public RotoItem
{
public:

    RotoLayer(const boost::shared_ptr<RotoContext>& context,
              const std::string & name,
              const boost::shared_ptr<RotoLayer>& parent);

    explicit RotoLayer(const RotoLayer & other);

    virtual ~RotoLayer();

    //void clone(const RotoLayer & other);

    /**
     * @brief Must be implemented by the derived class to save the state into
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void save(RotoItemSerialization* obj) const OVERRIDE;

    /**
     * @brief Must be implemented by the derived class to load the state from
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void load(const RotoItemSerialization & obj) OVERRIDE;

    ///only callable on the main-thread
    ///No check is done to figure out if the item already exists in this layer
    ///this is up to the caller responsability
    void addItem(const boost::shared_ptr<RotoItem>& item);

    ///Inserts the item into the layer before the indicated index.
    ///The same restrictions as addItem are applied.
    void insertItem(const boost::shared_ptr<RotoItem>& item,int index);

    ///only callable on the main-thread
    void removeItem(const boost::shared_ptr<RotoItem>& item);

    ///Returns the index of the given item in the layer, or -1 if not found
    int getChildIndex(const boost::shared_ptr<RotoItem>& item) const;

    ///only callable on the main-thread
    const std::list< boost::shared_ptr<RotoItem> >& getItems() const;

    ///MT-safe
    std::list< boost::shared_ptr<RotoItem> > getItems_mt_safe() const;

private:

    boost::scoped_ptr<RotoLayerPrivate> _imp;
};

/**
 * @class This class represents a bezier curve.
 * Note that the bezier also supports feather points.
 * This class is MT-safe
 *
 * The curve supports animation, and by default once the first control point is added the curve
 * has at least a minimum of 1 keyframe.
 **/


struct BezierPrivate;
class Bezier
    : public RotoDrawableItem
{
    Q_OBJECT

public:

    Bezier(const boost::shared_ptr<RotoContext>& context,
           const std::string & name,
           const boost::shared_ptr<RotoLayer>& parent);

    Bezier(const Bezier & other,
           const boost::shared_ptr<RotoLayer>& parent);

    virtual ~Bezier();

    virtual void clone(const RotoItem* other);
    /**
     * @brief Adds a new control point to the curve. A feather point will be added, at the same position.
     * If auto keying is enabled and this is the first point and there's no keyframe a new keyframe will be set at the current time.
     *
     * This function can only be called when the curved is not yet "finished". Once setCurveFinished has
     * been called you can no longer call this function either. The only way to make it finished again
     * is to call setCurveFinished. You can then add points normally with addControlPointAfterIndex
     * This function is used to build-up the curve as opposed to addControlPointAfterIndex which is there to
     * edit an already fully shaped spline.
     **/
    boost::shared_ptr<BezierCP> addControlPoint(double x,double y);

    /**
     * @brief Adds a new control point to the curve after the control point at the given index.
     * A feather point will be added, at the same position.
     * If auto keying is enabled, and there's no keyframe a new keyframe will be set at the current time.
     *
     * If index is -1 then the point will be added as the first point of the curve.
     * If index is invalid an invalid argument exception will be thrown.
     **/
    boost::shared_ptr<BezierCP> addControlPointAfterIndex(int index,double t);

    /**
     * @brief Returns the number of control points of the bezier
     **/
    int getControlPointsCount() const;

    /**
     * @brief Given the (x,y) coordinates of a point, this function returns whether a point lies on
     * the cubic bezier curve of the feather points or of the control points or not.
     * @returns The index of the starting control point (P0) of the bezier segment on which the given
     * point lies. If the point doesn't belong to any bezier segment of this curve then it will return -1.
     **/
    int isPointOnCurve(double x, double y, double acceptance, double *t, bool* feather) const;

    /**
     * @brief Set whether the curve is finished or not. A finished curve will have an arc between the first
     * and the last control point.
     **/
    void setCurveFinished(bool finished);
    bool isCurveFinished() const;

    /**
     * @brief Removes the control point at the given index if any. The feather point will also be removed , at the same position.
     * If auto keying is enabled, and there's no keyframe a new keyframe will be set at the current time.
     **/
    void removeControlPointByIndex(int index);

    /**
     * @brief Move the control point at the given index by the given dx and dy.
     * If feather link is enabled, the feather point will also be moved by the same deltas.
     * If  there's no keyframe at the current time, a new keyframe will be added.
     * This function asserts that  auto-keying is  enabled.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void movePointByIndex(int index,int time,double dx,double dy);

    /**
     * @brief Moves the feather point at the given index if any by the given dx and dy.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void moveFeatherByIndex(int index,int time,double dx,double dy);

    /**
     * @brief Moves the left bezier point of the control point at the given index by the given deltas
     * and at the given time.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void moveLeftBezierPoint(int index,int time,double dx,double dy);

    /**
     * @brief Moves the right bezier point of the control point at the given index by the given deltas
     * and at the given time.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void moveRightBezierPoint(int index,int time,double dx,double dy);

    /**
     * @brief Transforms the given point at the given time by the given matrix.
     **/
    void transformPoint(const boost::shared_ptr<BezierCP> & point,int time,Transform::Matrix3x3* matrix);

    /**
     * @brief Provided for convenience. It set the left bezier point of the control point at the given index to
     * the position given by (x,y) at the given time.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     * This function will also set the point of the feather point at the given x,y.
     **/
    void setLeftBezierPoint(int index,int time,double x,double y);

    /**
     * @brief Provided for convenience. It set the right bezier point of the control point at the given index to
     * the position given by (x,y) at the given time.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     * This function will also set the point of the feather point at the given x,y.
     **/
    void setRightBezierPoint(int index,int time,double x,double y);


    /**
     * @brief This function is a combinaison of setPosition + setLeftBezierPoint / setRightBeziePoint
     * It only works for feather points!
     **/
    void setPointAtIndex(bool feather,int index,int time,double x,double y,double lx,double ly,double rx,double ry);

    /**
     * @brief Set the left and right bezier point of the control point.
     **/
    void movePointLeftAndRightIndex(BezierCP & p,int time,double lx,double ly,double rx,double ry);


    /**
     * @brief Removes the feather point at the given index by making it equal the "true" control point.
     **/
    void removeFeatherAtIndex(int index);

    /**
     * @brief Smooth the curvature of the bezier at the given index by expanding the tangents.
     * This is also applied to the feather points.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void smoothPointAtIndex(int index,int time);

    /**
     * @brief Cusps the curvature of the bezier at the given index by reducing the tangents.
     * This is also applied to the feather points.
     * If auto keying is enabled and there's no keyframe at the current time, a new keyframe will be added.
     * If ripple edit is enabled, the point will be moved at the same location at all keyframes.
     **/
    void cuspPointAtIndex(int index,int time);

    /**
     * @brief Set a new keyframe at the given time. If a keyframe already exists this function does nothing.
     **/
    void setKeyframe(int time);

    /**
     * @brief Removes a keyframe at the given time if any.
     **/
    void removeKeyframe(int time);
    
    /**
     * @brief Moves a keyframe
     **/
    void moveKeyframe(int oldTime,int newTime);


    /**
     * @brief Returns the number of keyframes for this spline.
     **/
    int getKeyframesCount() const;

    /**
     * @brief Evaluates the spline at the given time and returns the list of all the points on the curve.
     * @param nbPointsPerSegment controls how many points are used to draw one Bezier segment
     **/
    void evaluateAtTime_DeCasteljau(int time,
                                    unsigned int mipMapLevel,
                                    int nbPointsPerSegment,
                                    std::list<Natron::Point>* points,
                                    RectD* bbox) const;

    /**
     * @brief Evaluates the bezier formed by the feather points. Segments which are equal to the control points of the bezier
     * will not be drawn.
     **/
    void evaluateFeatherPointsAtTime_DeCasteljau(int time,
                                                 unsigned int mipMapLevel,
                                                 int nbPointsPerSegment,
                                                 bool evaluateIfEqual,
                                                 std::list<Natron::Point >* points,
                                                 RectD* bbox) const;

    /**
     * @brief Returns the bounding box of the bezier. The last value computed by evaluateAtTime_DeCasteljau will be returned,
     * otherwise if it has never been called, evaluateAtTime_DeCasteljau will be called to compute the bounding box.
     **/
    RectD getBoundingBox(int time) const;

    /**
     * @brief Returns a const ref to the control points of the bezier curve. This can only ever be called on the main thread.
     **/
    const std::list< boost::shared_ptr<BezierCP> > & getControlPoints() const;
    std::list< boost::shared_ptr<BezierCP> > getControlPoints_mt_safe() const;

    /**
     * @brief Returns a const ref to the feather points of the bezier curve. This can only ever be called on the main thread.
     **/
    const std::list< boost::shared_ptr<BezierCP> > & getFeatherPoints() const;
    std::list< boost::shared_ptr<BezierCP> > getFeatherPoints_mt_safe() const;
    enum ControlPointSelectionPref
    {
        FEATHER_FIRST = 0,
        CONTROL_POINT_FIRST,
        WHATEVER_FIRST
    };

    /**
     * @brief Returns a pointer to a nearby control point if any. This function  also returns the feather point
     * The first member is the actual point nearby, and the second the counter part (i.e: either the feather point
     * if the first is a control point, or the other way around).
     **/
    std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> >
    isNearbyControlPoint(double x,double y,double acceptance,ControlPointSelectionPref pref,int* index) const;

    /**
     * @brief Given the control point in parameter, return its index in the curve's control points list.
     * If no such control point could be found, -1 is returned.
     **/
    int getControlPointIndex(const boost::shared_ptr<BezierCP> & cp) const;
    int getControlPointIndex(const BezierCP* cp) const;

    /**
     * @brief Given the feather point in parameter, return its index in the curve's feather points list.
     * If no such feather point could be found, -1 is returned.
     **/
    int getFeatherPointIndex(const boost::shared_ptr<BezierCP> & fp) const;

    /**
     * @brief Returns the control point at the given index if any, NULL otherwise.
     **/
    boost::shared_ptr<BezierCP> getControlPointAtIndex(int index) const;

    /**
     * @brief Returns the feather point at the given index if any, NULL otherwise.
     **/
    boost::shared_ptr<BezierCP> getFeatherPointAtIndex(int index) const;
    boost::shared_ptr<BezierCP> getFeatherPointForControlPoint(const boost::shared_ptr<BezierCP> & cp) const;
    boost::shared_ptr<BezierCP> getControlPointForFeatherPoint(const boost::shared_ptr<BezierCP> & fp) const;

    /**
     * @brief Returns all the control points/feather points within the rectangle
     * given by (l,r,b,t). Each control point is paired with its counter part.
     * The first member is always "the selected point" and the second the counter part.
     * @param mode An integer representing what the function should do.
     * mode == 0: Add all the cp/fp within the rect and their counter parts in the return value
     * mode == 1: Add only the cp within the rect and their respective feather points counter parts
     * mode == 2: Add only the fp within the rect and their repsective control points counter parts
     **/
    std::list< std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> > >
    controlPointsWithinRect(double l,double r,double b,double t,double acceptance,int mode) const;
    static void leftDerivativeAtPoint(int time,const BezierCP & p,const BezierCP & prev,double *dx,double *dy);
    static void rightDerivativeAtPoint(int time,const BezierCP & p,const BezierCP & next,double *dx,double *dy);

    /**
     * @brief Computes the location of the feather extent relative to the current feather point position and
     * the given feather distance.
     * In the case the control point and the feather point of the bezier are distinct, this function just makes use
     * of Thales theorem.
     * If the feather point and the control point are equal then this function computes the left and right derivative
     * of the bezier at that point to determine the direction in which the extent is.
     * @returns The delta from the given feather point to apply to find out the extent position.
     *
     * Note that the delta will be applied to fp.
     **/
    static Natron::Point expandToFeatherDistance(const Natron::Point & cp, //< the point
                                                 Natron::Point* fp, //< the feather point
                                                 double featherDistance, //< feather distance
                                                 const std::list<Natron::Point> & featherPolygon, //< the polygon of the bezier
                                                 const RectD & featherPolyBBox, //< helper to speed-up pointInPolygon computations
                                                 int time, //< time
                                                 std::list<boost::shared_ptr<BezierCP> >::const_iterator prevFp, //< iterator pointing to the feather before curFp
                                                 std::list<boost::shared_ptr<BezierCP> >::const_iterator curFp, //< iterator pointing to fp
                                                 std::list<boost::shared_ptr<BezierCP> >::const_iterator nextFp); //< iterator pointing after curFp
    enum FillRule
    {
        OddEvenFill,
        WindingFill
    };

#pragma message WARN("pointInPolygon should not be used, see comment")
    /*
       The pointInPolygon function should not be used.
       The algorithm to know which side is the outside of a polygon consists in computing the global polygon orientation.
       To compute the orientation, compute its surface. If positive the polygon is clockwise, if negative it's counterclockwise.
       to compute the surface, take the starting point of the polygon, and imagine a fan made of all the triangles
       pointing at this point. The surface of a tringle is half the cross-product of two of its sides issued from
       the same point (the starting point of the polygon, in this case.
       The orientation of a polygon has to be computed only once for each modification of the polygon (whenever it's edited), and
       should be stored with the polygon.
       Of course an 8-shaped polygon doesn't have an outside, but it still has an orientation. The feather direction
       should follow this orientation.
     */
    static bool pointInPolygon(const Natron::Point & p,const std::list<Natron::Point> & polygon,
                               const RectD & featherPolyBBox,FillRule rule);

    /**
     * @brief Must be implemented by the derived class to save the state into
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void save(RotoItemSerialization* obj) const OVERRIDE;

    /**
     * @brief Must be implemented by the derived class to load the state from
     * the serialization object.
     * Derived implementations must call the parent class implementation.
     **/
    virtual void load(const RotoItemSerialization & obj) OVERRIDE;

    void getKeyframeTimes(std::set<int> *times) const;

    /**
     * @brief Get the nearest previous keyframe from the given time.
     * If nothing was found INT_MIN is returned.
     **/
    int getPreviousKeyframeTime(int time) const;

    /**
     * @brief Get the nearest next keyframe from the given time.
     * If nothing was found INT_MAX is returned.
     **/
    int getNextKeyframeTime(int time) const;

signals:

    void aboutToClone();

    void cloned();

    void keyframeSet(int time);

    void keyframeRemoved(int time);
    
    void controlPointAdded();
    
    void controlPointRemoved();

private:

    boost::scoped_ptr<BezierPrivate> _imp;
};


/**
 * @class This class is a member of all effects instantiated in the context "paint". It describes internally
 * all the splines data structures and their state.
 * This class is MT-safe.
 **/
class RotoContextSerialization;
struct RotoContextPrivate;
class RotoContext
: public QObject, public boost::enable_shared_from_this<RotoContext>
{
    Q_OBJECT

public:

    enum SelectionReason
    {
        OVERLAY_INTERACT = 0, ///when the user presses an interact
        SETTINGS_PANEL, ///when the user interacts with the settings panel
        OTHER ///when the project loader restores the selection
    };

    RotoContext(Natron::Node* node);

    virtual ~RotoContext();
    
    void createBaseLayer();

    /**
     * @brief Returns true when the context is empty (it has no shapes)
     **/
    bool isEmpty() const;

    /**
     * @brief Turn on/off auto-keying
     **/
    void setAutoKeyingEnabled(bool enabled);
    bool isAutoKeyingEnabled() const;

    /**
     * @brief Turn on/off feather link. When enabled the feather point will move by the same delta as the corresponding spline point
     * is moved by.
     **/
    void setFeatherLinkEnabled(bool enabled);
    bool isFeatherLinkEnabled() const;

    /**
     * @brief Turn on/off ripple edit. When enabled moving a point at a given keyframe will move it to the same position
     * at all keyframes.
     **/
    void setRippleEditEnabled(bool enabled);
    bool isRippleEditEnabled() const;

    int getTimelineCurrentTime() const;

    /**
     * @brief Add a new layer to the currently selected layer.
     **/
    boost::shared_ptr<RotoLayer> addLayer();
    void addLayer(const boost::shared_ptr<RotoLayer> & layer);

    /**
     * @brief Make a new bezier curve and append it into the currently selected layer.
     * @param baseName A hint to name the item. It can be something like "Bezier", "Ellipse", "Rectangle" , etc...
     **/
    boost::shared_ptr<Bezier> makeBezier(double x,double y,const std::string & baseName);

    /**
     * @brief Removes the given item from the context. This also removes the item from the selection
     * if it was selected. If the item has children, this will also remove all the children.
     **/
    void removeItem(const boost::shared_ptr<RotoItem>& item, SelectionReason reason = OTHER);

    ///This is here for undo/redo purpose. Do not call this
    void addItem(const boost::shared_ptr<RotoLayer>& layer, int indexInLayer, const boost::shared_ptr<RotoItem> & item,SelectionReason reason);
    /**
     * @brief Returns a const ref to the layers list. This can only be called from
     * the main thread.
     **/
    const std::list< boost::shared_ptr<RotoLayer> > & getLayers() const;

    /**
     * @brief Returns a bezier curves nearby the point (x,y) and the parametric value
     * which would be used to find the exact bezier point lying on the curve.
     **/
    boost::shared_ptr<Bezier> isNearbyBezier(double x,double y,double acceptance,int* index,double* t,bool *feather) const;


    /**
     * @brief Returns the region of definition of the shape unioned to the region of definition of the node
     * or the project format.
     **/
    void getMaskRegionOfDefinition(int time,
                                   int view,
                                   RectD* rod) const; //!< rod in canonical coordinates

    /**
     * @brief Render the mask formed by all the shapes contained in the context within the roi.
     * The image will use the cache if byPassCache is set to true.
     **/
    boost::shared_ptr<Natron::Image> renderMask(bool useCache,
                                                const RectI & roi,
                                                Natron::ImageComponentsEnum components,
                                                U64 nodeHash,
                                                U64 ageToRender,
                                                const RectD & nodeRoD,
                                                SequenceTime time,
                                                Natron::ImageBitDepthEnum depth,
                                                int view,
                                                unsigned int mipmapLevel,
                                                const std::list<boost::shared_ptr<Natron::Image> >& inputImages,
                                                bool byPassCache);

    /**
     * @brief To be called when a change was made to trigger a new render.
     **/
    void evaluateChange();

    /**
     *@brief Returns the age of the roto context
     **/
    U64 getAge();

    ///Serialization
    void save(RotoContextSerialization* obj) const;

    ///Deserialization
    void load(const RotoContextSerialization & obj);


    /**
     * @brief This must be called by the GUI whenever an item is selected. This is recursive for layers.
     **/
    void select(const boost::shared_ptr<RotoItem> & b,RotoContext::SelectionReason reason);

    ///for convenience
    void select(const std::list<boost::shared_ptr<Bezier> > & beziers,RotoContext::SelectionReason reason);
    void select(const std::list<boost::shared_ptr<RotoItem> > & items,RotoContext::SelectionReason reason);

    /**
     * @brief This must be called by the GUI whenever an item is deselected. This is recursive for layers.
     **/
    void deselect(const boost::shared_ptr<RotoItem> & b,RotoContext::SelectionReason reason);

    ///for convenience
    void deselect(const std::list<boost::shared_ptr<Bezier> > & beziers,RotoContext::SelectionReason reason);
    void deselect(const std::list<boost::shared_ptr<RotoItem> > & items,RotoContext::SelectionReason reason);

    void clearSelection(RotoContext::SelectionReason reason);

    ///only callable on main-thread
    void setKeyframeOnSelectedCurves();

    ///only callable on main-thread
    void removeKeyframeOnSelectedCurves();

    ///only callable on main-thread
    void goToPreviousKeyframe();

    ///only callable on main-thread
    void goToNextKeyframe();

    /**
     * @brief Returns a list of the currently selected curves. Can only be called on the main-thread.
     **/
    std::list< boost::shared_ptr<Bezier> > getSelectedCurves() const;


    /**
     * @brief Returns a const ref to the selected items. This can only be called on the main thread.
     **/
    const std::list< boost::shared_ptr<RotoItem> > & getSelectedItems() const;

    /**
     * @brief Returns a list of all the curves in the order in which they should be rendered.
     * Non-active curves will not be inserted into the list.
     * MT-safe
     **/
    std::list< boost::shared_ptr<Bezier> > getCurvesByRenderOrder() const;
    
    int getNCurves() const;
    
    boost::shared_ptr<RotoLayer> getLayerByName(const std::string & n) const;
    boost::shared_ptr<RotoItem> getItemByName(const std::string & n) const;
    boost::shared_ptr<RotoItem> getLastInsertedItem() const;

#ifdef NATRON_ROTO_INVERTIBLE
    boost::shared_ptr<Bool_Knob> getInvertedKnob() const;
#endif
    boost::shared_ptr<Color_Knob> getColorKnob() const;


    void setLastItemLocked(const boost::shared_ptr<RotoItem> & item);
    boost::shared_ptr<RotoItem> getLastItemLocked() const;
    boost::shared_ptr<RotoLayer> getDeepestSelectedLayer() const;

    void onItemLockedChanged(const boost::shared_ptr<RotoItem>& item);

    void emitRefreshViewerOverlays();

    void getBeziersKeyframeTimes(std::list<int> *times) const;

    /**
     * @brief Returns the name of the node holding this item
     **/
    std::string getRotoNodeName() const;
    
    void onItemNameChanged(const boost::shared_ptr<RotoItem>& item);

signals:

    /**
     * Emitted when the selection is changed. The integer corresponds to the
     * RotoContext::SelectionReason enum.
     **/
    void selectionChanged(int);

    void restorationComplete();

    void itemInserted(int);

    void itemRemoved(const boost::shared_ptr<RotoItem>&,int);

    void refreshViewerOverlays();

    void itemLockedChanged();
    
    void itemNameChanged(const boost::shared_ptr<RotoItem>&);

public slots:

    void onAutoKeyingChanged(bool enabled);

    void onFeatherLinkChanged(bool enabled);

    void onRippleEditChanged(bool enabled);
    
    void onSelectedKnobCurveChanged();

private:

    void selectInternal(const boost::shared_ptr<RotoItem>& b);
    void deselectInternal(boost::shared_ptr<RotoItem> b);

    void removeItemRecursively(const boost::shared_ptr<RotoItem>& item,SelectionReason reason);

    /**
     * @brief First searches through the selected layer which one is the deepest in the hierarchy.
     * If nothing is found, it searches through the selected items and find the deepest selected item's layer
     **/
    boost::shared_ptr<RotoLayer> findDeepestSelectedLayer() const;
    boost::scoped_ptr<RotoContextPrivate> _imp;
};

#endif // KNOBROTO_H
