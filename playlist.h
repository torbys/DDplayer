#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QWidget>
#include <QStringList>
#include <QSettings>
#include <QListWidgetItem>

class ThumbnailExtractor;
class VideoListItem;

namespace Ui {
class PlayList;
}

class PlayList : public QWidget
{
    Q_OBJECT

public:
    explicit PlayList(QWidget *parent = nullptr);
    ~PlayList();

    QString currentFilePath() const;
    QStringList fileList() const;

signals:
    void sigPlayFile(const QString &filePath);
    void sigAddFile(const QString &filePath);
    void sigNavButtonStateChanged(bool canPre, bool canNext);

public slots:
    void onAddFile();
    void onItemClicked(int index);
    void clearHistory();
    void playPrevious();
    void playNext();

private slots:
    void on_videoList_currentRowChanged(int currentRow);
    void on_videoList_itemDoubleClicked(QListWidgetItem *item);
    void onThumbnailReady(const QString &filePath, const QImage &image);
    void onItemDeleted(const QString &filePath);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void updateListDisplay();
    void updateNavButtonState();

    VideoListItem* createListItem(const QString &title, const QString &filePath);

    Ui::PlayList *ui;
    QStringList filePaths_;
    int currentIndex_ = -1;
    QString lastDir_;

    ThumbnailExtractor *thumbnailExtractor_;

    static constexpr const char* SETTINGS_GROUP = "PlayList";
    static constexpr const char* SETTINGS_KEY_FILES = "files";
    static constexpr const char* SETTINGS_KEY_LASTDIR = "lastDir";
};

#endif // PLAYLIST_H
