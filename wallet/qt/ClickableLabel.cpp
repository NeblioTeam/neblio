#include "ClickableLabel.h"

#include <QMouseEvent>

ClickableLabel::ClickableLabel(QWidget* parent, Qt::WindowFlags /*f*/) : QLabel(parent) {}

ClickableLabel::~ClickableLabel() {}

void ClickableLabel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}
