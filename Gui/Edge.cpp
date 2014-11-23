//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#include "Edge.h"

#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QGraphicsScene>

#include "Gui/NodeGui.h"
#include "Engine/Node.h"
#include "Engine/Settings.h"
#include "Engine/ViewerInstance.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846264338327950288   /* pi             */
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923132169163975144   /* pi/2           */
#endif

#define EDGE_LENGTH_MIN 0.1

#define kGraphicalContainerOffset 10 \
    //!< number of offset pixels from the arrow that determine if a click is contained in the arrow or not

Edge::Edge(int inputNb_,
           double angle_,
           const boost::shared_ptr<NodeGui> & dest_,
           QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
      , _isOutputEdge(false)
      , _inputNb(inputNb_)
      , _angle(angle_)
      , _label(NULL)
      , _arrowHead()
      , _dest(dest_)
      , _source()
      , _defaultColor(Qt::black)
      , _renderingColor(243,149,0)
      , _useRenderingColor(false)
      , _useHighlight(false)
      , _paintWithDash(false)
      , _paintBendPoint(false)
      , _bendPointHiddenAutomatically(false)
      , _wasLabelVisible(true)
      , _middlePoint()
{
    setPen( QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin) );
    if ( (_inputNb != -1) && _dest ) {
        _label = new QGraphicsTextItem(QString( _dest->getNode()->getInputLabel(_inputNb).c_str() ),this);
        _label->setDefaultTextColor( QColor(200,200,200) );
    }
    setAcceptedMouseButtons(Qt::LeftButton);
    initLine();
    setFlag(QGraphicsItem::ItemStacksBehindParent);
    setZValue(4);
    if ( _dest && _dest->getNode()->getLiveInstance() && _dest->getNode()->getLiveInstance()->isInputOptional(_inputNb) ) {
        _paintWithDash = true;
    }
}

Edge::Edge(const boost::shared_ptr<NodeGui> & src,
           QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
      , _isOutputEdge(true)
      , _inputNb(-1)
      , _angle(M_PI_2)
      , _label(NULL)
      , _arrowHead()
      , _dest()
      , _source(src)
      , _defaultColor(Qt::black)
      , _renderingColor(243,149,0)
      , _useRenderingColor(false)
      , _useHighlight(false)
      , _paintWithDash(false)
{
    assert(src);
    setPen( QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin) );
    setAcceptedMouseButtons(Qt::LeftButton);
    initLine();
    setFlag(QGraphicsItem::ItemStacksBehindParent);
    setZValue(4);
}

Edge::~Edge()
{
    if (_dest) {
        _dest->markInputNull(this);
    }
}

void
Edge::setSource(const boost::shared_ptr<NodeGui> & src)
{
    _source = src;
    initLine();
}

void
Edge::setSourceAndDestination(const boost::shared_ptr<NodeGui> & src,
                              const boost::shared_ptr<NodeGui> & dst)
{
    _source = src;
    _dest = dst;
    if (!_label) {
        _label = new QGraphicsTextItem(QString( _dest->getNode()->getInputLabel(_inputNb).c_str() ),this);
        _label->setDefaultTextColor( QColor(200,200,200) );
    } else {
        _label->setPlainText( QString( _dest->getNode()->getInputLabel(_inputNb).c_str() ) );
    }
    if ( _dest && _dest->getNode()->getLiveInstance() && _dest->getNode()->getLiveInstance()->isInputOptional(_inputNb) ) {
        _paintWithDash = true;
    }
    initLine();
}

void
Edge::setOptional(bool optional)
{
    _paintWithDash = optional;
}

void
Edge::setUseHighlight(bool highlight)
{
    _useHighlight = highlight;
    update();
}

static void
makeEdges(const QRectF & bbox,
          std::vector<QLineF> & edges)
{
    QPointF topLeft = bbox.topLeft();

    edges.push_back( QLineF( topLeft.x() + bbox.width(), // right
                             topLeft.y(),
                             topLeft.x() + bbox.width(),
                             topLeft.y() + bbox.height() ) );

    edges.push_back( QLineF( topLeft.x() + bbox.width(), // bottom
                             topLeft.y() + bbox.height(),
                             topLeft.x(),
                             topLeft.y() + bbox.height() ) );

    edges.push_back( QLineF( topLeft.x(),  // left
                             topLeft.y() + bbox.height(),
                             topLeft.x(),
                             topLeft.y() ) );

    edges.push_back( QLineF( topLeft.x(), // top
                             topLeft.y(),
                             topLeft.x() + bbox.width(),
                             topLeft.y() ) );
}

void
Edge::initLine()
{
    if (!_source && !_dest) {
        return;
    }

    double sc = scale();
    QRectF sourceBBOX = _source ? mapFromItem( _source.get(), _source->boundingRect() ).boundingRect() : QRectF(0,0,1,1);
    QRectF destBBOX = _dest ? mapFromItem( _dest.get(), _dest->boundingRect() ).boundingRect()  : QRectF(0,0,1,1);
    QSize dstNodeSize;
    QSize srcNodeSize;
    if (_dest) {
        dstNodeSize = QSize( destBBOX.width(),destBBOX.height() );
    }
    if (_source) {
        srcNodeSize = QSize( sourceBBOX.width(),sourceBBOX.height() );
    }

    QPointF dst;

    if (_dest) {
        dst = destBBOX.center();
    } else if (_source && !_dest) {
        dst = QPointF( sourceBBOX.x(),sourceBBOX.y() ) + QPointF(srcNodeSize.width() / 2., srcNodeSize.height() + 10);
    }

    std::vector<QLineF> dstEdges;
    std::vector<QLineF> srcEdges;
    if (_dest) {
        makeEdges(destBBOX, dstEdges);
    }
    if (_source) {
        makeEdges(sourceBBOX, srcEdges);
    }

    QPointF srcpt;
    if (_source && _dest) {
        /////// This is a connected edge, either input or output
        srcpt = sourceBBOX.center();

        setLine( dst.x(),dst.y(),srcpt.x(),srcpt.y() );

        bool foundIntersection = false;
        QPointF dstIntersection;
        for (int i = 0; i < 4; ++i) {
            QLineF::IntersectType type = dstEdges[i].intersect(line(), &dstIntersection);
            if (type == QLineF::BoundedIntersection) {
                setLine( QLineF( dstIntersection,line().p2() ) );
                foundIntersection = true;
                break;
            }
        }
        QPointF srcInteresect;

        if (foundIntersection) {
            ///Find the intersection with the source bbox
            foundIntersection = false;
            for (int i = 0; i < 4; ++i) {
                QLineF::IntersectType type = srcEdges[i].intersect(line(), &srcInteresect);
                if (type == QLineF::BoundedIntersection) {
                    foundIntersection = true;
                    break;
                }
            }
        }
        if (foundIntersection) {
            _middlePoint = (srcInteresect + dstIntersection) / 2;
            ///Hide bend point for short edges
            double visibleLength  = QLineF(srcInteresect,dstIntersection).length();
            if ( (visibleLength < 50) && _paintBendPoint ) {
                _paintBendPoint = false;
                _bendPointHiddenAutomatically = true;
            } else if ( (visibleLength >= 50) && _bendPointHiddenAutomatically ) {
                _bendPointHiddenAutomatically = false;
                _paintBendPoint = true;
            }


            if ( _label && isActive() ) {
                _label->setPos( _middlePoint + QPointF(-5,-10) );
                QFontMetrics fm( _label->font() );
                int fontHeight = fm.height();
                double txtWidth = fm.width( _label->toPlainText() );
                if ( (visibleLength < fontHeight * 2) || (visibleLength < txtWidth) ) {
                    _label->hide();
                } else {
                    _label->show();
                }
            }
        }
    } else if (!_source && _dest) {
        ///// The edge is an input edge which is unconnected
        srcpt = QPointF( dst.x() + (std::cos(_angle) * 100000 * sc),
                         dst.y() - (std::sin(_angle) * 100000 * sc) );
        setLine( dst.x(),dst.y(),srcpt.x(),srcpt.y() );

        ///ok now that we have the direction between dst and srcPt we can get the distance between the center of the node
        ///and the intersection with the bbox. We add UNATTECHED_ARROW_LENGTH to that distance to position srcPt correctly.
        QPointF intersection;
        bool foundIntersection = false;
        for (int i = 0; i < 4; ++i) {
            QLineF::IntersectType type = dstEdges[i].intersect(line(), &intersection);
            if (type == QLineF::BoundedIntersection) {
                setLine( QLineF( intersection,line().p2() ) );
                foundIntersection = true;
                break;
            }
        }

        assert(foundIntersection);
        double distToCenter = std::sqrt( ( intersection.x() - dst.x() ) * ( intersection.x() - dst.x() ) +
                                         ( intersection.y() - dst.y() ) * ( intersection.y() - dst.y() ) );
        distToCenter += appPTR->getCurrentSettings()->getDisconnectedArrowLength();

        srcpt = QPointF( dst.x() + (std::cos(_angle) * distToCenter * sc),
                         dst.y() - (std::sin(_angle) * distToCenter * sc) );
        setLine( dst.x(),dst.y(),srcpt.x(),srcpt.y() );

        if (_label) {
            double cosinus = std::cos(_angle);
            int yOffset = 0;
            if (cosinus < 0) {
                yOffset = -40;
            } else if ( (cosinus >= -0.01) && (cosinus <= 0.01) ) {
                yOffset = +5;
            } else {
                yOffset = +10;
            }

            /*adjusting dst to show label at the middle of the line*/

            QPointF labelDst = QPointF( destBBOX.x(),destBBOX.y() ) + QPointF(dstNodeSize.width() / 2.,0);

            _label->setPos( ( ( labelDst.x() + srcpt.x() ) / 2. ) + yOffset,( labelDst.y() + srcpt.y() ) / 2. - 20 );
        }
    } else if (_source && !_dest) {
        ///// The edge is an output edge which is unconnected
        srcpt = QPointF( sourceBBOX.x(),sourceBBOX.y() ) + QPointF(srcNodeSize.width() / 2.,srcNodeSize.height() / 2.);
        setLine( dst.x(),dst.y(),srcpt.x(),srcpt.y() );

        ///output edges don't have labels
    }


    double length = std::max(EDGE_LENGTH_MIN, line().length());


    ///This is the angle the edge forms with the X axis
    qreal a = std::acos(line().dx() / length);

    if (line().dy() >= 0) {
        a = 2 * M_PI - a;
    }

    qreal arrowSize = 5. * sc;
    QPointF arrowP1 = line().p1() + QPointF(std::sin(a + M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(std::sin(a + M_PI - M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI - M_PI / 3) * arrowSize);

    _arrowHead.clear();
    _arrowHead << dst << arrowP1 << arrowP2;
} // initLine

QPainterPath
Edge::shape() const
{
    QPainterPath path = QGraphicsLineItem::shape();

    path.addPolygon(_arrowHead);


    return path;
}

static inline double
sqr(double x)
{
    return x * x;
}

static double
dist2(const QPointF & p1,
      const QPointF & p2)
{
    return sqr( p2.x() - p1.x() ) +  sqr( p2.y() - p1.y() );
}

static double
dist2ToSegment(const QLineF & line,
               const QPointF & p)
{
    double length2 = line.length() * line.length();
    const QPointF & p1 = line.p1();
    const QPointF &p2 = line.p2();

    if (length2 == 0.) {
        return dist2(p, p1);
    }
    // Consider the line extending the segment, parameterized as p1 + t (p2 - p1).
    // We find projection of point p onto the line.
    // It falls where t = [(p-p1) . (p2-p1)] / |p2-p1|^2
    double t = ( ( p.x() - p1.x() ) * ( p2.x() - p1.x() ) + ( p.y() - p1.y() ) * ( p2.y() - p1.y() ) ) / length2;
    if (t < 0) {
        return dist2(p, p1);
    }
    if (t > 1) {
        return dist2(p, p2);
    }

    return dist2( p, QPointF( p1.x() + t * ( p2.x() - p1.x() ),
                              p1.y() + t * ( p2.y() - p1.y() ) ) );
}

bool
Edge::contains(const QPointF &point) const
{
    double d2 = dist2ToSegment(line(), point);

    return d2 <= kGraphicalContainerOffset * kGraphicalContainerOffset;
}

void
Edge::dragSource(const QPointF & src)
{
    setLine( QLineF(line().p1(),src) );

    double a = std::acos( line().dx() / std::max(EDGE_LENGTH_MIN, line().length()) );
    if (line().dy() >= 0) {
        a = 2 * M_PI - a;
    }

    double arrowSize = 5;
    QPointF arrowP1 = line().p1() + QPointF(std::sin(a + M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(std::sin(a + M_PI - M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI - M_PI / 3) * arrowSize);
    _arrowHead.clear();
    _arrowHead << line().p1() << arrowP1 << arrowP2;


    if (_label) {
        _label->setPos( QPointF( ( ( line().p1().x() + src.x() ) / 2. ) - 5,( ( line().p1().y() + src.y() ) / 2. ) - 5 ) );
    }
}

void
Edge::dragDest(const QPointF & dst)
{
    setLine( QLineF( dst,line().p2() ) );

    double a = std::acos( line().dx() / std::max(EDGE_LENGTH_MIN, line().length()) );
    if (line().dy() >= 0) {
        a = 2 * M_PI - a;
    }

    double arrowSize = 5;
    QPointF arrowP1 = line().p1() + QPointF(std::sin(a + M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(std::sin(a + M_PI - M_PI / 3) * arrowSize,
                                            std::cos(a + M_PI - M_PI / 3) * arrowSize);
    _arrowHead.clear();
    _arrowHead << line().p1() << arrowP1 << arrowP2;
}

void
Edge::setBendPointVisible(bool visible)
{
    _paintBendPoint = visible;
    if (!visible) {
        _bendPointHiddenAutomatically = false;
    }
    update();
}

bool
Edge::isNearbyBendPoint(const QPointF & scenePoint)
{
    assert(_source && _dest);
    QPointF pos = mapFromScene(scenePoint);
    if ( ( pos.x() >= (_middlePoint.x() - 10) ) && ( pos.x() <= (_middlePoint.x() + 10) ) &&
         ( pos.y() >= (_middlePoint.y() - 10) ) && ( pos.y() <= (_middlePoint.y() + 10) ) ) {
        return true;
    }

    return false;
}

void
Edge::setVisibleDetails(bool visible)
{
    if (!visible) {
        _wasLabelVisible = _label->isVisible();
        _label->hide();
    } else {
        if (_wasLabelVisible) {
            _label->show();
        }
    }
}

void
Edge::paint(QPainter *painter,
            const QStyleOptionGraphicsItem * /*options*/,
            QWidget * /*parent*/)
{
    QPen myPen = pen();

    if (_paintWithDash) {
        QVector<qreal> dashStyle;
        qreal space = 4;
        dashStyle << 3 << space;
        myPen.setDashPattern(dashStyle);
    } else {
        myPen.setStyle(Qt::SolidLine);
    }

    QColor color;
    if (_useHighlight) {
        color = Qt::green;
    } else if (_useRenderingColor) {
        color = _renderingColor;
    } else {
        color = _defaultColor;
    }
    myPen.setColor(color);
    painter->setPen(myPen);
    QLineF l = line();
    painter->drawLine(l);

    myPen.setStyle(Qt::SolidLine);
    painter->setPen(myPen);

    QPainterPath headPath;
    headPath.addPolygon(_arrowHead);
    headPath.closeSubpath();
    painter->fillPath(headPath, color);

    if (_paintBendPoint) {
        QRectF arcRect(_middlePoint.x() - 5,_middlePoint.y() - 5,10,10);
        QPainterPath bendPointPath;
        bendPointPath.addEllipse(arcRect);
        bendPointPath.closeSubpath();
        painter->fillPath(bendPointPath,Qt::yellow);
    }
}

LinkArrow::LinkArrow(const NodeGui* master,
                     const NodeGui* slave,
                     QGraphicsItem* parent)
    : QObject(), QGraphicsLineItem(parent)
      , _master(master)
      , _slave(slave)
      , _arrowHead()
      , _renderColor(Qt::black)
      , _headColor(Qt::white)
      , _lineWidth(1)
{
    QObject::connect( master,SIGNAL( positionChanged(int,int) ),this,SLOT( refreshPosition() ) );
    QObject::connect( slave,SIGNAL( positionChanged(int,int) ),this,SLOT( refreshPosition() ) );

    refreshPosition();
    setZValue(0);
}

LinkArrow::~LinkArrow()
{
}

void
LinkArrow::setColor(const QColor & color)
{
    _renderColor = color;
}

void
LinkArrow::setArrowHeadColor(const QColor & headColor)
{
    _headColor = headColor;
}

void
LinkArrow::setWidth(int lineWidth)
{
    _lineWidth = lineWidth;
}

void
LinkArrow::refreshPosition()
{
    QRectF bboxSlave = mapFromItem( _slave,_slave->boundingRect() ).boundingRect();

    ///like the box master in kfc! was bound to name it so I'm hungry atm
    QRectF boxMaster = mapFromItem( _master,_master->boundingRect() ).boundingRect();
    QPointF dst = boxMaster.center();
    QPointF src = bboxSlave.center();

    setLine( QLineF(src,dst) );

    ///Get the intersections of the line with the nodes
    std::vector<QLineF> masterEdges;
    std::vector<QLineF> slaveEdges;
    makeEdges(bboxSlave, slaveEdges);
    makeEdges(boxMaster, masterEdges);


    QPointF masterIntersect;
    QPointF slaveIntersect;
    bool foundIntersection = false;
    for (int i = 0; i < 4; ++i) {
        QLineF::IntersectType type = slaveEdges[i].intersect(line(), &slaveIntersect);
        if (type == QLineF::BoundedIntersection) {
            foundIntersection = true;
            break;
        }
    }

    if (!foundIntersection) {
        ///Don't bother continuing, there's no intersection that means the line is contained in the node bbox

        ///hence that the 2 nodes are overlapping on the nodegraph so probably we wouldn't see the link anyway.
        return;
    }

    foundIntersection = false;
    for (int i = 0; i < 4; ++i) {
        QLineF::IntersectType type = masterEdges[i].intersect(line(), &masterIntersect);
        if (type == QLineF::BoundedIntersection) {
            foundIntersection = true;
            break;
        }
    }
    if (!foundIntersection) {
        ///Don't bother continuing, there's no intersection that means the line is contained in the node bbox

        ///hence that the 2 nodes are overlapping on the nodegraph so probably we wouldn't see the link anyway.
        return;
    }

    ///Now get the middle of the visible portion of the link
    QPointF middle = (masterIntersect + slaveIntersect) / 2.;
    double length = std::max(EDGE_LENGTH_MIN, line().length());
    ///This is the angle the edge forms with the X axis
    qreal a = std::acos(line().dx() / length);

    if (line().dy() >= 0) {
        a = 2 * M_PI - a;
    }

    qreal arrowSize = 10. * scale();
    QPointF arrowP1 = middle + QPointF(std::sin(a + M_PI / 3) * arrowSize,
                                       std::cos(a + M_PI / 3) * arrowSize);
    QPointF arrowP2 = middle + QPointF(std::sin(a + M_PI - M_PI / 3) * arrowSize,
                                       std::cos(a + M_PI - M_PI / 3) * arrowSize);

    _arrowHead.clear();
    _arrowHead << middle << arrowP1 << arrowP2;
} // refreshPosition

void
LinkArrow::paint(QPainter *painter,
                 const QStyleOptionGraphicsItem* /*options*/,
                 QWidget* /*parent*/)
{
    QPen myPen = pen();

    myPen.setColor(_renderColor);
    myPen.setWidth(_lineWidth);
    painter->setPen(myPen);
    QLineF l = line();
    painter->drawLine(l);

    myPen.setStyle(Qt::SolidLine);
    painter->setPen(myPen);

    QPainterPath headPath;
    headPath.addPolygon(_arrowHead);
    headPath.closeSubpath();
    painter->fillPath(headPath, _headColor);
}

