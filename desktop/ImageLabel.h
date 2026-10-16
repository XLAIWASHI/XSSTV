#pragma once

#include <QLabel>
#include <QPixmap>

// 按比例显示图片的标签：窗口变化时自动重新缩放，不拉伸变形。
// 关键：它不参与"决定布局大小"，只接受布局给它的空间，避免加载图片时版面跳动。
class ImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ImageLabel(QWidget* parent = nullptr);

    void setImage(const QImage& img);  // 传空图 = 清空
    void clearImage();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void rescale();

    QPixmap original_;
};
