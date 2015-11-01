#include <QPainter>
#include "imagesample.h"

ImageSample::ImageSample()
    : QQuickPaintedItem()
    , sample_()
{

}

ImageSample::~ImageSample()
{

}

void ImageSample::paint(QPainter *painter)
{
    if (sample_.size().isEmpty())
        return;

    float aspect_ratio = sample_.width() / sample_.height();
    int w  = height() * aspect_ratio;
    int x = (width() - w) / 2;

    painter->setViewport(x, 0, w, height());
    painter->drawImage(QRectF(0, 0, width(), height()), sample_);
}

const QImage &ImageSample::sample() const
{
    return sample_;
}

void ImageSample::setSample(const QImage &sample)
{
    sample_ = sample;
    update();
}



