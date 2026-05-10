#ifndef THUMBNAIL_EXTRACTOR_H
#define THUMBNAIL_EXTRACTOR_H

#include <QString>
#include <QImage>
#include <QObject>

class ThumbnailExtractor : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailExtractor(QObject *parent = nullptr);
    ~ThumbnailExtractor();

    static QImage extractFirstFrame(const QString &filePath, int maxWidth = 160, int maxHeight = 90);

signals:
    void thumbnailReady(const QString &filePath, const QImage &image);

public slots:
    void extractAsync(const QString &filePath, int maxWidth = 160, int maxHeight = 90);

private:
};

#endif // THUMBNAIL_EXTRACTOR_H
