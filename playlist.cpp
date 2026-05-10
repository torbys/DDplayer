#include "playlist.h"
#include "ui_playlist.h"
#include "video_list_item.h"
#include "thumbnail_extractor.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>
#include <QListWidgetItem>
#include <QDir>

PlayList::PlayList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayList)
{
    ui->setupUi(this);

    thumbnailExtractor_ = new ThumbnailExtractor(this);
    connect(thumbnailExtractor_, &ThumbnailExtractor::thumbnailReady,
            this, &PlayList::onThumbnailReady);

    connect(ui->addBtn, &QPushButton::clicked, this, &PlayList::onAddFile);
    connect(ui->videoList, &QListWidget::currentRowChanged,
            this, &PlayList::on_videoList_currentRowChanged);
    connect(ui->videoList, &QListWidget::itemDoubleClicked,
            this, &PlayList::on_videoList_itemDoubleClicked);

    loadSettings();
}

PlayList::~PlayList()
{
    saveSettings();
    delete ui;
}

VideoListItem* PlayList::createListItem(const QString &title, const QString &filePath)
{
    VideoListItem *item = new VideoListItem(title, filePath);
    connect(item, &VideoListItem::sigDeleteRequested, this, &PlayList::onItemDeleted);
    return item;
}

void PlayList::onAddFile()
{
    QString defaultPath = lastDir_.isEmpty() ?
                              QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) :
                              lastDir_;

    QStringList filters;
    filters << "视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.m3u8)"
            << "所有文件 (*.*)";

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择视频文件"),
        defaultPath,
        filters.join(";;")
        );

    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo info(filePath);
    lastDir_ = info.absolutePath();

    if (filePaths_.contains(filePath)) {
        int existingIndex = filePaths_.indexOf(filePath);
        ui->videoList->setCurrentRow(existingIndex);
        onItemClicked(existingIndex);
        return;
    }

    filePaths_.append(filePath);
    currentIndex_ = filePaths_.size() - 1;

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QListWidgetItem *listItem = new QListWidgetItem(ui->videoList);
    listItem->setSizeHint(QSize(0, 64));

    VideoListItem *widget = createListItem(fileName, filePath);
    ui->videoList->setItemWidget(listItem, widget);
    ui->videoList->setCurrentRow(currentIndex_);

    emit sigAddFile(filePath);
    emit sigPlayFile(filePath);

    thumbnailExtractor_->extractAsync(filePath, 100, 56);

    updateNavButtonState();
    saveSettings();
}

void PlayList::onItemClicked(int index)
{
    if (index < 0 || index >= filePaths_.size()) {
        return;
    }

    currentIndex_ = index;

    for (int i = 0; i < ui->videoList->count(); i++) {
        QListWidgetItem *item = ui->videoList->item(i);
        VideoListItem *widget = static_cast<VideoListItem*>(ui->videoList->itemWidget(item));
        if (widget) {
            widget->setSelected(i == index);
        }
    }

    QString filePath = filePaths_.at(index);
    qDebug() << "切换到视频:" << filePath;
    emit sigPlayFile(filePath);

    updateNavButtonState();
}

QString PlayList::currentFilePath() const
{
    if (currentIndex_ >= 0 && currentIndex_ < filePaths_.size()) {
        return filePaths_.at(currentIndex_);
    }
    return QString();
}

QStringList PlayList::fileList() const
{
    return filePaths_;
}

void PlayList::loadSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);

    QStringList savedFiles = settings.value(SETTINGS_KEY_FILES).toStringList();
    lastDir_ = settings.value(SETTINGS_KEY_LASTDIR).toString();

    if (lastDir_.isEmpty() || !QDir(lastDir_).exists()) {
        lastDir_ = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    }

    for (const QString &path : savedFiles) {
        QFileInfo info(path);
        if (info.exists() && info.isFile()) {
            filePaths_.append(path);

            QString fileName = info.fileName();
            QListWidgetItem *listItem = new QListWidgetItem(ui->videoList);
            listItem->setSizeHint(QSize(0, 64));

            VideoListItem *widget = createListItem(fileName, path);
            ui->videoList->setItemWidget(listItem, widget);

            thumbnailExtractor_->extractAsync(path, 100, 56);
        }
    }

    settings.endGroup();

    if (!filePaths_.isEmpty()) {
        currentIndex_ = 0;
        ui->videoList->setCurrentRow(0);

        QListWidgetItem *firstItem = ui->videoList->item(0);
        if (firstItem) {
            VideoListItem *widget = static_cast<VideoListItem*>(ui->videoList->itemWidget(firstItem));
            if (widget) {
                widget->setSelected(true);
            }
        }
    }

    for (int i = 0; i < ui->videoList->count(); ++i) {
        ui->videoList->item(i)->setSizeHint(QSize(-1, 400));
    }
}

void PlayList::saveSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    settings.setValue(SETTINGS_KEY_FILES, filePaths_);
    settings.setValue(SETTINGS_KEY_LASTDIR, lastDir_);
    settings.endGroup();
}

void PlayList::clearHistory()
{
    filePaths_.clear();
    ui->videoList->clear();
    currentIndex_ = -1;
    updateNavButtonState();
    saveSettings();
}

void PlayList::on_videoList_currentRowChanged(int currentRow)
{
    if (currentRow >= 0 && currentRow < filePaths_.size()) {
        onItemClicked(currentRow);
    }
}

void PlayList::on_videoList_itemDoubleClicked(QListWidgetItem *item)
{
    int row = ui->videoList->row(item);
    if (row >= 0) {
        onItemClicked(row);
    }
}

void PlayList::onThumbnailReady(const QString &filePath, const QImage &image)
{
    int idx = filePaths_.indexOf(filePath);
    if (idx < 0 || idx >= ui->videoList->count())
        return;

    QListWidgetItem *item = ui->videoList->item(idx);
    if (!item)
        return;

    VideoListItem *widget = static_cast<VideoListItem*>(ui->videoList->itemWidget(item));
    if (widget) {
        widget->setThumbnail(image);
    }
}

void PlayList::onItemDeleted(const QString &filePath)
{
    int idx = filePaths_.indexOf(filePath);
    if (idx < 0)
        return;

    filePaths_.removeAt(idx);

    QListWidgetItem *listItem = ui->videoList->takeItem(idx);
    if (listItem) {
        //delete listItem->widget();
        delete listItem;
    }

    if (filePaths_.isEmpty()) {
        currentIndex_ = -1;
    } else if (currentIndex_ >= filePaths_.size()) {
        currentIndex_ = filePaths_.size() - 1;
    } else if (idx <= currentIndex_) {
        currentIndex_--;
    }

    if (currentIndex_ >= 0 && currentIndex_ < ui->videoList->count()) {
        ui->videoList->setCurrentRow(currentIndex_);
        for (int i = 0; i < ui->videoList->count(); i++) {
            QListWidgetItem *item = ui->videoList->item(i);
            VideoListItem *widget = static_cast<VideoListItem*>(ui->videoList->itemWidget(item));
            if (widget) {
                widget->setSelected(i == currentIndex_);
            }
        }
    }

    updateNavButtonState();
    saveSettings();
}

void PlayList::playPrevious()
{
    if (currentIndex_ > 0) {
        onItemClicked(currentIndex_ - 1);
    }
}

void PlayList::playNext()
{
    if (currentIndex_ >= 0 && currentIndex_ < filePaths_.size() - 1) {
        onItemClicked(currentIndex_ + 1);
    }
}

void PlayList::updateNavButtonState()
{
    bool canPre = (currentIndex_ > 0);
    bool canNext = (currentIndex_ >= 0 && currentIndex_ < filePaths_.size() - 1);
    emit sigNavButtonStateChanged(canPre, canNext);
}
