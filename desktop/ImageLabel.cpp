#include "ImageLabel.h"

#include <QResizeEvent>

ImageLabel::ImageLabel(QWidget* parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    // Ignored：尺寸完全由布局决定，不让图片的尺寸反过来撑大布局
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setFrameShape(QFrame::Box);
    setText("（无图片）");
}

void ImageLabel::setImage(const QImage& img)
{
    original_ = img.isNull() ? QPixmap() : QPixmap::fromImage(img);
    rescale();
}

void ImageLabel::clearImage()
{
    original_ = QPixmap();
    rescale();
}

QSize ImageLabel::sizeHint() const
{
    return QSize(60, 40); // 固定很小，不随图片变化
}

QSize ImageLabel::minimumSizeHint() const
{
    return QSize(60, 40);
}

void ImageLabel::resizeEvent(QResizeEvent* e)
{
    QLabel::resizeEvent(e);
    rescale();
}

void ImageLabel::rescale()
{
    if (original_.isNull())
    {
        setPixmap(QPixmap());
        setText("（无图片）");
        return;
    }
    setPixmap(original_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
