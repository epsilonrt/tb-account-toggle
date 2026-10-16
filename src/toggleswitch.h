#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

// Windows 11 like toggle switch (accent-colored track, sliding knob).
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    // Set the state without animation (initial state)
    void setCheckedImmediate(bool checked);

    qreal knobPos() const { return m_pos; }
    void setKnobPos(qreal pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateTo(bool checked);

    qreal m_pos = 0.0;
    bool m_keyboardFocus = false;
    QPropertyAnimation *m_anim;
};
