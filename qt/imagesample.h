#ifndef IMAGESAMPLE_H
#define IMAGESAMPLE_H

#include <QObject>
#include <QQuickPaintedItem>
#include <QImage>
#include <QPainter>
#include "player.h"

class ImageSample : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QImage sample READ sample WRITE setSample)
public:
    ImageSample();
    ~ImageSample();
    void paint(QPainter *painter);

    const QImage &sample() const;
    void setSample(const QImage &sample);

private:
    QImage sample_;
};

#endif // IMAGESAMPLE_H
