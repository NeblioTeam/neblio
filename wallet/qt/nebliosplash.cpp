#include "nebliosplash.h"

#include "ui_interface.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>

NeblioSplash::NeblioSplash(QWidget* parent) : QWidget(parent)
{
    setWindowTitle("Neblio starting...");

    mainLayout = new QGridLayout;

    setLayout(mainLayout);

    neblioLogoLabel = new QLabel(this);
    mainProgressBar = new QProgressBar(this);
    mainTextLabel   = new QLabel(this);

    QPixmap logoPixmap(":/images/splash");
    neblioLogoLabel->setPixmap(logoPixmap);

    mainLayout->addWidget(neblioLogoLabel, 0, 0);
    mainLayout->addWidget(mainProgressBar, 1, 0);
    mainLayout->addWidget(mainTextLabel, 2, 0);

    neblioLogoLabel->setMargin(0);
    mainTextLabel->setMargin(3);
    mainProgressBar->setContentsMargins(5, 0, 5, 0);
    mainLayout->setMargin(0);

    mainProgressBar->setValue(0);

    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

    mainTextLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    move(pos() + QGuiApplication::primaryScreen()->geometry().center() - geometry().center());
    setMaximumSize(this->size());
}

void NeblioSplash::showMessage(const QString& message, int progress)
{
    mainTextLabel->setText(message);
    mainProgressBar->setValue(progress);
}

void NeblioSplash::mousePressEvent(QMouseEvent* event)
{
    startPos = event->pos();
    QWidget::mousePressEvent(event);
}

void NeblioSplash::mouseMoveEvent(QMouseEvent* event)
{
    QPoint   delta = event->pos() - startPos;
    QWidget* w     = window();
    if (w) {
        w->move(w->pos() + delta);
    }
    QWidget::mouseMoveEvent(event);
}
