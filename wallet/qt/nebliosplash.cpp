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
    mainTextLabel->setMargin(5);
    mainTextLabel->setStyleSheet("background-color:#291f3a; color:#0bdfd4");
    mainProgressBar->setContentsMargins(5, 0, 5, 0);
    const QString progressBarStyle = "QProgressBar {border: 0px; text-align: center; background-color: "
                                     "#291f3a; color:#ffffff; padding: 0px 10px 0px 10px;} "
                                     "QProgressBar::chunk {background-color: #0bdfd4; width: 1px;}";
    mainProgressBar->setStyleSheet(progressBarStyle);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

    mainProgressBar->setValue(0);
    mainProgressBar->setMaximum(std::numeric_limits<int>::max());

    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

    mainTextLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    setMaximumSize(this->size());
}

void NeblioSplash::showMessage(const QString& message, double progressFromZeroToOne)
{
    mainTextLabel->setText(message);
    if (progressFromZeroToOne >= 1.) {
        mainProgressBar->setValue(std::numeric_limits<int>::max());
    } else if (progressFromZeroToOne <= 0.) {
        mainProgressBar->setValue(0);
    } else {
        const int progress = static_cast<int>(static_cast<double>(std::numeric_limits<int>::max()) *
                                              static_cast<double>(progressFromZeroToOne));
        mainProgressBar->setValue(progress);
    }
}

void NeblioSplash::moveWidgetToScreenCenter()
{
    move(pos() + QGuiApplication::primaryScreen()->geometry().center() - geometry().center());
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
