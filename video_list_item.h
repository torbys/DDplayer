#ifndef VIDEO_LIST_ITEM_H
#define VIDEO_LIST_ITEM_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QImage>

class VideoListItem : public QWidget
{
    Q_OBJECT

public:
    explicit VideoListItem(const QString &title, const QString &filePath, QWidget *parent = nullptr);
    ~VideoListItem();

    void setThumbnail(const QImage &image);
    void setSelected(bool selected);
    QString filePath() const { return filePath_; }

signals:
    void sigDeleteRequested(const QString &filePath);

private slots:
    void onDeleteClicked();

private:
    QLabel *thumbnailLabel_;
    QLabel *titleLabel_;
    QPushButton *deleteBtn_;
    QString filePath_;

};

#endif // VIDEO_LIST_ITEM_H
