#ifndef SOCIALLINKS_H
#define SOCIALLINKS_H

#include "ClickableLabel.h"
#include <QGridLayout>
#include <QPixmap>
#include <QWidget>

class SocialLinks : public QWidget
{
    Q_OBJECT

    QGridLayout*    mainLayout;
    ClickableLabel* discordLabel;
    ClickableLabel* facebookLabel;
    ClickableLabel* githubLabel;
    ClickableLabel* instagramLabel;
    ClickableLabel* redditLabel;
    ClickableLabel* telegramLabel;
    ClickableLabel* twitterLabel;
    ClickableLabel* youtubeLabel;

    ClickableLabel* makeLabelOfLogo(const QString& iconPath, const QString& link);

    void openURL(const QString& url);

public:
    explicit SocialLinks(QWidget* parent = nullptr);

signals:
};

#endif // SOCIALLINKS_H
