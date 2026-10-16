#include "toggleswitch.h"

#include <QFocusEvent>
#include <QPainter>
#include <QPropertyAnimation>

namespace {
constexpr int TrackW = 40;
constexpr int TrackH = 20;
} // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent), m_anim(new QPropertyAnimation(this, "knobPos", this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    m_anim->setDuration(140);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animateTo);
}

QSize ToggleSwitch::sizeHint() const
{
    return {TrackW + 4, TrackH + 4};
}

void ToggleSwitch::setCheckedImmediate(bool checked)
{
    setChecked(checked);
    m_anim->stop();
    setKnobPos(checked ? 1.0 : 0.0);
}

void ToggleSwitch::setKnobPos(qreal pos)
{
    m_pos = pos;
    update();
}

void ToggleSwitch::animateTo(bool checked)
{
    m_anim->stop();
    m_anim->setStartValue(m_pos);
    m_anim->setEndValue(checked ? 1.0 : 0.0);
    m_anim->start();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const QPalette pal = palette();
    const bool on = isChecked();
    const bool enabled = isEnabled();
    const QColor text = pal.color(QPalette::WindowText);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(2, (height() - TrackH) / 2.0, TrackW, TrackH);
    const qreal radius = TrackH / 2.0;

    if (on) {
        p.setPen(Qt::NoPen);
        p.setBrush(enabled ? pal.color(QPalette::Accent) : pal.color(QPalette::Mid));
    } else {
        QColor border = text;
        border.setAlphaF(enabled ? 0.6f : 0.3f);
        p.setPen(border);
        p.setBrush(Qt::NoBrush);
    }
    p.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // Knob: grows slightly when on or hovered, like Windows 11
    qreal d = on ? 12 : 10;
    if (underMouse() && enabled)
        d += 2;
    const qreal x0 = track.left() + radius;
    const qreal x1 = track.right() - radius;
    const qreal cx = x0 + (x1 - x0) * m_pos;
    QColor knob = on ? pal.color(QPalette::Base) : text;
    if (!on)
        knob.setAlphaF(enabled ? 0.75f : 0.35f);
    p.setPen(Qt::NoPen);
    p.setBrush(knob);
    p.drawEllipse(QRectF(cx - d / 2, track.center().y() - d / 2, d, d));

    if (hasFocus() && m_keyboardFocus) {
        p.setPen(text);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-2, -2, 2, 2), radius + 2, radius + 2);
    }
}

void ToggleSwitch::focusInEvent(QFocusEvent *event)
{
    // Focus ring only for keyboard navigation, as in Windows 11
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
    update();
    QAbstractButton::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
    update();
    QAbstractButton::leaveEvent(event);
}
