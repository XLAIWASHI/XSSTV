#pragma once

#include <QWidget>
#include <QVector>

class TitleBar;
class QLabel;
class QPushButton;
class QStackedWidget;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QShowEvent;
class EncodePage;
class DecodePage;

// 主窗口（无边框 + 半透明圆角）：
// 顶部自定义标题栏 / 导航（编码、解码、资源管理）/ 内容区 / 底部状态栏。
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void setPage(int index);
    void toggleMaximize();

    TitleBar* titleBar_ = nullptr;
    QVector<QPushButton*> nav_;
    QStackedWidget* stack_ = nullptr;
    QLabel* status_ = nullptr;
    EncodePage* encPage_ = nullptr;
    DecodePage* decPage_ = nullptr;
    bool acrylicDone_ = false;
};
