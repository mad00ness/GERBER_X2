#include "bridgeitem.h"
#include "itemgroup.h"

#include <QApplication>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <gccreator.h>
#include <graphicsview.h>
#include <limits>
#include <math.h>
#include <scene.h>

BridgeItem::BridgeItem(double& lenght, double& size, GCode::SideOfMilling& side, BridgeItem*& ptr)
    : m_ptr(ptr)
    , m_lenght(lenght)
    , m_size(size)
    , m_side(side)
{
    connect(GraphicsView::self, &GraphicsView::mouseMove, this, &BridgeItem::setNewPos);
    m_path.addEllipse(QPointF(), m_lenght / 2, m_lenght / 2);
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges);
    setZValue(std::numeric_limits<double>::max());
}

QRectF BridgeItem::boundingRect() const
{
    return m_path.boundingRect();
}

void BridgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    painter->setBrush(!m_ok ? Qt::red : Qt::green);
    painter->setTransform(QTransform().rotate(-(m_angle - 360)), true);
    painter->setPen(Qt::NoPen);
    painter->drawPath(m_path);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(Qt::white, 2 * GraphicsView::scaleFactor()));

    const double halfSize = m_size / 2;

    QLineF l(0, 0, m_lenght / 2 + halfSize, 0);
    switch (m_side) {
    case GCode::On:
        break;
    case GCode::Outer:
        l.translate(halfSize, 0);
        break;
    case GCode::Inner:
        l.translate(-halfSize, 0);
        break;
    }

    auto drawEllipse = [painter, halfSize, this](const QPointF& pt, bool fl = false) {
        const QRectF rectangle(pt + QPointF{ halfSize, halfSize }, pt - QPointF{ halfSize, halfSize });
        const int startAngle = (fl ? 0 : 180) * 16;
        const int spanAngle = 180 * 16;
        painter->drawArc(rectangle, startAngle, spanAngle);
    };

    l.setAngle(+90);
    drawEllipse(l.p2());
    l.setAngle(-90);
    drawEllipse(l.p2(), true);
}

void BridgeItem::setNewPos(const QPointF& pos) { setPos(pos); }

QVariant BridgeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        return calculate(value.toPointF());
    } else
        return QGraphicsItem::itemChange(change, value);
}

void BridgeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_lastPos = pos();
    disconnect(GraphicsView::self, &GraphicsView::mouseMove, this, &BridgeItem::setNewPos);
    QGraphicsItem::mousePressEvent(event);
}

QPointF BridgeItem::calculate(const QPointF& pos)
{
    QList<QGraphicsItem*> col(scene()->collidingItems(this));
    if (col.isEmpty())
        return pos;

    QPointF pt;
    double l = std::numeric_limits<double>::max();
    double lastAngle = 0.0;
    for (QGraphicsItem* item : col) {
        GraphicsItem* gi = dynamic_cast<GraphicsItem*>(item);
        if (gi && (gi->type() == GiShapeC || gi->type() == GiDrill || gi->type() == GiGerber || gi->type() == GiRaw)) {
            if (!gi->isSelected())
                continue;
            for (const Path& path : gi->paths()) {
                for (int i = 0, s = path.size(); i < s; ++i) {
                    const QLineF l1(pos, toQPointF(path[i]));
                    const QLineF l2(pos, toQPointF(path[(i + 1) % s]));
                    const QLineF l3(toQPointF(path[(i + 1) % s]), toQPointF(path[i]));
                    if (lastAngle == 0.0)
                        lastAngle = l3.normalVector().angle();
                    const double p = (l1.length() + l2.length() + l3.length()) / 2;
                    if (l1.length() < l3.length() && l2.length() < l3.length()) {
                        const double h = (2 / l3.length()) * sqrt(p * (p - l1.length()) * (p - l2.length()) * (p - l3.length()));
                        if (l > h) {
                            l = h;
                            QLineF line(toQPointF(path[i]), toQPointF(path[(i + 1) % s]));
                            line.setLength(sqrt(l1.length() * l1.length() - h * h));
                            pt = line.p2();
                            m_angle = line.normalVector().angle();
                        }
                    }
                    lastAngle = l3.normalVector().angle();
                }
            }
        }
    }
    if (l < m_lenght / 2) {
        m_ok = true;
        return pt;
    }
    m_ok = false;
    return pos;
}

double BridgeItem::angle() const { return m_angle; }

void BridgeItem::update()
{
    m_path = QPainterPath();
    m_path.addEllipse(QPointF(), m_lenght / 2, m_lenght / 2);
    QGraphicsItem::update();
}

IntPoint BridgeItem::getPoint(const int side) const
{
    QLineF l2(0, 0, m_size / 2, 0);
    l2.translate(pos());
    switch (side) {
    case GCode::On:
        return toIntPoint(pos());
    case GCode::Outer:
        l2.setAngle(m_angle + 180);
        return toIntPoint(l2.p2());
    case GCode::Inner:
        l2.setAngle(m_angle);
        return toIntPoint(l2.p2());
    }
    return IntPoint();
}

QLineF BridgeItem::getPath() const
{
    QLineF retLine(QLineF::fromPolar(m_size * 0.51, m_angle).p2(), QLineF::fromPolar(m_size * 0.51, m_angle + 180).p2());
    retLine.translate(pos());
    return retLine;
}

double BridgeItem::lenght() const { return m_lenght; }

bool BridgeItem::ok() const { return m_ok; }

QPainterPath BridgeItem::shape() const { return m_path; }

void BridgeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* /*event*/) { deleteLater(); }

void BridgeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_ok && pos() == m_lastPos) {
        m_ptr = new BridgeItem(m_lenght, m_size, m_side, m_ptr);
        scene()->addItem(m_ptr);
    } else if (!m_ok) {
        deleteLater();
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

Paths BridgeItem::paths() const { return Paths(); }

int BridgeItem::type() const { return GiBridge; }
