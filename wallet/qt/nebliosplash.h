#ifndef NEBLIOSPLASH_H
#define NEBLIOSPLASH_H

#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QWidget>

class NeblioSplash : public QWidget
{
    Q_OBJECT

    QGridLayout*  mainLayout;
    QLabel*       neblioLogoLabel;
    QProgressBar* mainProgressBar;
    QLabel*       mainTextLabel;

    void   mousePressEvent(QMouseEvent* event) override;
    void   mouseMoveEvent(QMouseEvent* event) override;
    QPoint startPos;

public:
    explicit NeblioSplash(QWidget* parent = nullptr);

    void showMessage(const QString& message, double progressFromZeroToOne);
};

#endif // NEBLIOSPLASH_H
