#include "video_list_item.h"
#include <QPixmap>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>

VideoListItem::VideoListItem(const QString &title, const QString &filePath, QWidget *parent)
    : QWidget(parent)
    , filePath_(filePath)
{
    setFixedHeight(60);
    setStyleSheet("background-color: transparent; border: none;");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(10);

    thumbnailLabel_ = new QLabel(this);
    thumbnailLabel_->setFixedSize(100, 56);
    thumbnailLabel_->setScaledContents(true);
    thumbnailLabel_->setStyleSheet(
        "background-color: #2a2a2a;"
        "border: 1px solid #444;"
        "border-radius: 3px;"
    );
    layout->addWidget(thumbnailLabel_);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setWordWrap(true);
    titleLabel_->setStyleSheet(
        "color: #cccccc;"
        "font-size: 12px;"
        "background-color: transparent;"
        "border: none;"
    );
    layout->addWidget(titleLabel_, 1);

    deleteBtn_ = new QPushButton("×", this);
    deleteBtn_->setFixedSize(24, 24);
    deleteBtn_->setCursor(Qt::PointingHandCursor);
    deleteBtn_->setToolTip(tr("删除此记录"));
    deleteBtn_->setStyleSheet(
        "QPushButton {"
        "   color: #888888;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "   background-color: transparent;"
        "   border: none;"
        "   border-radius: 12px;"
        "}"
        "QPushButton:hover {"
        "   color: #ffffff;"
        "   background-color: #e74c3c;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #c0392b;"
        "}"
    );
    connect(deleteBtn_, &QPushButton::clicked, this, &VideoListItem::onDeleteClicked);
    layout->addWidget(deleteBtn_);
}

VideoListItem::~VideoListItem()
{
}

void VideoListItem::onDeleteClicked()
{
    emit sigDeleteRequested(filePath_);
}

void VideoListItem::setThumbnail(const QImage &image)
{
    if (image.isNull()) {
        QPixmap placeholder(thumbnailLabel_->size());
        placeholder.fill(QColor("#2a2a2a"));
        thumbnailLabel_->setPixmap(placeholder);
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image.scaled(
        thumbnailLabel_->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    ));

    QPixmap rounded(pixmap.size());
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rounded.rect(), 3, 3);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, pixmap);
    painter.end();

    thumbnailLabel_->setPixmap(rounded);
}

void VideoListItem::setSelected(bool selected)
{
    if (selected) {
        setStyleSheet("background-color: #FF8A00; border-radius: 4px; margin: 2px 4px;");
        titleLabel_->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px; background-color: transparent; border: none;");
        deleteBtn_->setStyleSheet(
            "QPushButton {"
            "   color: #ffffff;"
            "   font-size: 16px;"
            "   font-weight: bold;"
            "   background-color: transparent;"
            "   border: none;"
            "   border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "   color: #ffffff;"
            "   background-color: #c0392b;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #a93226;"
            "}"
        );
    } else {
        setStyleSheet("background-color: transparent; border: none;");
        titleLabel_->setStyleSheet("color: #cccccc; font-size: 12px; background-color: transparent; border: none;");
        deleteBtn_->setStyleSheet(
            "QPushButton {"
            "   color: #888888;"
            "   font-size: 16px;"
            "   font-weight: bold;"
            "   background-color: transparent;"
            "   border: none;"
            "   border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "   color: #ffffff;"
            "   background-color: #e74c3c;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #c0392b;"
            "}"
        );
    }
}
