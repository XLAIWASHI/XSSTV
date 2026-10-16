#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

// 自定义标题栏：拖动窗口、双击最大化/还原，右侧三个窗口按钮
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(const QString& title, QWidget* parent = nullptr);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    QLabel* titleLabel_ = nullptr;
};
