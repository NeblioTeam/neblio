#include "sociallinks.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMessageBox>
#include <QUrl>

const char* DiscordLink   = "https://discord.gg/PFt3bVw";
const char* FacebookLink  = "https://www.facebook.com/neblioteam";
const char* GithubLink    = "https://github.com/NeblioTeam/neblio";
const char* InstagramLink = "https://www.instagram.com/neblio.official";
const char* RedditLink    = "https://www.reddit.com/r/Neblio";
const char* TelegramLink  = "https://t.me/neblio_official";
const char* TwitterLink   = "https://twitter.com/NeblioTeam";
const char* YoutubeLink   = "https://www.youtube.com/neblio";

ClickableLabel* SocialLinks::makeLabelOfLogo(const QString& iconPath, const QString& link)
{
    ClickableLabel* label = new ClickableLabel(this);
    QPixmap         redditPixmap(iconPath);
    redditPixmap = redditPixmap.scaledToHeight(32, Qt::SmoothTransformation);
    label->setPixmap(redditPixmap);
    label->setToolTip("Click to visit: " + link);
    connect(label, &ClickableLabel::clicked, this, [&, link]() { openURL(link); });

    return label;
}

void SocialLinks::openURL(const QString& url)
{
    if (!QDesktopServices::openUrl(QUrl(url))) {
        QMessageBox::StandardButton res =
            QMessageBox::warning(this, "Could not find browser",
                                 "Failed to open URL. Would you like to copy the link to clipboard?",
                                 QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
                                 QMessageBox::StandardButton::Yes);
        if (res == QMessageBox::StandardButton::Yes) {
            QClipboard* clipboard = QGuiApplication::clipboard();
            clipboard->setText(url);
        }
    }
}

SocialLinks::SocialLinks(QWidget* parent) : QWidget(parent)
{
    mainLayout = new QGridLayout;

    discordLabel   = makeLabelOfLogo(":icons/discord-logo", DiscordLink);
    facebookLabel  = makeLabelOfLogo(":icons/facebook-logo", FacebookLink);
    githubLabel    = makeLabelOfLogo(":icons/github-logo", GithubLink);
    instagramLabel = makeLabelOfLogo(":icons/instagram-logo", InstagramLink);
    redditLabel    = makeLabelOfLogo(":icons/reddit-logo", RedditLink);
    telegramLabel  = makeLabelOfLogo(":icons/telegram-logo", TelegramLink);
    twitterLabel   = makeLabelOfLogo(":icons/twitter-logo", TwitterLink);
    youtubeLabel   = makeLabelOfLogo(":icons/youtube-logo", YoutubeLink);

    mainLayout->addWidget(discordLabel, 0, 0);
    mainLayout->addWidget(facebookLabel, 0, 1);
    mainLayout->addWidget(githubLabel, 0, 2);
    mainLayout->addWidget(instagramLabel, 0, 3);
    mainLayout->addWidget(redditLabel, 0, 4);
    mainLayout->addWidget(telegramLabel, 0, 5);
    mainLayout->addWidget(twitterLabel, 0, 6);
    mainLayout->addWidget(youtubeLabel, 0, 7);

    this->setLayout(mainLayout);
}
