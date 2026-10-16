#include "MainWindow.h"

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWindow>

#include "DecodePage.h"
#include "EncodePage.h"
#include "ResourcePage.h"
#include "TitleBar.h"

#ifdef Q_OS_WIN
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

namespace
{
    // 用未公开的 SetWindowCompositionAttribute 开启 Acrylic 磨砂（Win10/11）
    void enableAcrylicBlur(HWND hwnd)
    {
        struct ACCENT_POLICY { int AccentState; int AccentFlags; int GradientColor; int AnimationId; };
        struct WINDOWCOMPOSITIONATTRIBDATA { int Attrib; void* pvData; size_t cbData; };
        enum { WCA_ACCENT_POLICY = 19, ACCENT_ENABLE_ACRYLICBLURBEHIND = 4 };

        using Fn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
        static Fn fn = reinterpret_cast<Fn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
        if (!fn) return;

        ACCENT_POLICY policy{};
        policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
        policy.AccentFlags = 0;
        policy.GradientColor = 0xB0312723; // AABBGGRR：约 70% 不透明，深灰(35,39,49)

        WINDOWCOMPOSITIONATTRIBDATA data{};
        data.Attrib = WCA_ACCENT_POLICY;
        data.pvData = &policy;
        data.cbData = sizeof(policy);
        fn(hwnd, &data);
    }
}
#endif

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    // 无边框 + 半透明背景（真正透出桌面）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("XSSTV");
    resize(900, 650);
    setAcceptDrops(true);

    titleBar_ = new TitleBar("XSSTV", this);

    // 自定义导航
    auto* navLayout = new QHBoxLayout();
    navLayout->setContentsMargins(16, 4, 16, 0);
    navLayout->setSpacing(6);
    const QStringList names = { "编码", "解码", "资源管理" };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = new QPushButton(names[i], this);
        b->setObjectName("navButton");
        b->setCheckable(true);
        b->setFlat(true);
        nav_.append(b);
        navLayout->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { setPage(i); });
    }
    navLayout->addStretch();

    // 页面
    stack_ = new QStackedWidget(this);
    encPage_ = new EncodePage(stack_);
    decPage_ = new DecodePage(stack_);
    auto* res = new ResourcePage(stack_);
    stack_->addWidget(encPage_);
    stack_->addWidget(decPage_);
    stack_->addWidget(res);

    status_ = new QLabel("就绪", this);
    status_->setObjectName("statusBar");
    status_->setFixedHeight(32);
    status_->setContentsMargins(14, 0, 14, 0);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(titleBar_);
    root->addLayout(navLayout);
    root->addWidget(stack_, 1);
    root->addWidget(status_);

    connect(titleBar_, &TitleBar::minimizeClicked, this, &MainWindow::showMinimized);
    connect(titleBar_, &TitleBar::maximizeClicked, this, &MainWindow::toggleMaximize);
    connect(titleBar_, &TitleBar::closeClicked, this, &MainWindow::close);
    connect(encPage_, &EncodePage::status, this, [this](const QString& s) { status_->setText(s); });
    connect(decPage_, &DecodePage::status, this, [this](const QString& s) { status_->setText(s); });

    // 视觉：参考配色（半透明深灰 + 选中深蓝 + 浅灰白按钮黑字）
    setStyleSheet(R"(
        QWidget { color: white; font-size: 13px; }
        #appIcon { color: #7fd7ff; font-weight: bold; margin-right: 4px; }
        #appTitle { font-weight: bold; }
        #statusBar { color: rgba(255,255,255,160); }

        /* 左侧/顶部导航按钮：透明底，选中=深蓝 */
        QPushButton#navButton {
            background: transparent; border: none;
            color: rgba(255,255,255,170);
            padding: 6px 16px; border-radius: 6px;
        }
        QPushButton#navButton:hover { background: rgba(255,255,255,18); }
        QPushButton#navButton:checked { background: #2f5d9e; color: white; }

        /* 操作按钮：浅灰白底 + 黑字 */
        QPushButton {
            background: #e6e8ee; color: #1a1c22;
            border: 1px solid #cfd3dc; border-radius: 8px; padding: 6px 14px;
        }
        QPushButton:hover { background: #f2f4f8; }
        QPushButton:disabled { background: rgba(230,232,238,70); color: rgba(30,32,40,120); }

        /* 输入框：浅灰边框 */
        QLineEdit {
            background: rgba(255,255,255,20); color: white;
            border: 1px solid rgba(200,205,215,160); border-radius: 6px; padding: 4px 6px;
        }

        /* 标题栏窗口按钮 */
        QToolButton { background: transparent; border: none; color: white; }
        QToolButton:hover { background: rgba(255,255,255,30); border-radius: 4px; }
        #closeButton:hover { background: rgba(220,60,60,200); }

        /* 分组框 */
        QGroupBox {
            border: 1px solid rgba(200,205,215,90);
            border-radius: 8px; margin-top: 12px; padding-top: 6px;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 10px; padding: 0 4px;
            color: rgba(255,255,255,200);
        }

        /* 下拉框 */
        QComboBox {
            background: rgba(255,255,255,20); color: white;
            border: 1px solid rgba(200,205,215,160); border-radius: 6px; padding: 4px 8px;
        }
        QComboBox QAbstractItemView {
            background: #232731; color: white;
            selection-background-color: #2f5d9e; border: 1px solid rgba(200,205,215,120);
        }

        /* 表格（资源管理） */
        QTableWidget {
            background: rgba(255,255,255,10);
            alternate-background-color: rgba(255,255,255,6);
            gridline-color: rgba(255,255,255,25);
            border: 1px solid rgba(200,205,215,80);
            border-radius: 8px;
        }
        QTableWidget::item:selected { background: #2f5d9e; color: white; }
        QHeaderView::section {
            background: rgba(255,255,255,20); color: white;
            border: none; padding: 4px 6px;
        }
    )");

    setPage(0);

    // 恢复上次的窗口大小/位置和所在页
    QSettings st;
    restoreGeometry(st.value("ui/geometry").toByteArray());
    setPage(st.value("ui/page", 0).toInt());
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    QSettings st;
    st.setValue("ui/geometry", saveGeometry());
    st.setValue("ui/page", stack_->currentIndex());
    QWidget::closeEvent(e);
}

void MainWindow::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
#ifdef Q_OS_WIN
    if (!acrylicDone_)
    {
        acrylicDone_ = true;
        enableAcrylicBlur(reinterpret_cast<HWND>(winId()));
    }
#endif
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    const QList<QUrl> urls = e->mimeData()->urls();
    if (urls.isEmpty()) return;

    const QString path = urls.first().toLocalFile();
    if (path.isEmpty()) return;

    const QString suffix = QFileInfo(path).suffix().toLower();
    static const QStringList imgExt = { "png", "jpg", "jpeg", "bmp" };

    if (imgExt.contains(suffix))
    {
        setPage(0); // 编码页
        encPage_->setInputImage(path);
        status_->setText("已拖入图片：" + QFileInfo(path).fileName());
        e->acceptProposedAction();
    }
    else if (suffix == "wav")
    {
        setPage(1); // 解码页
        decPage_->setInputWav(path);
        status_->setText("已拖入 WAV：" + QFileInfo(path).fileName());
        e->acceptProposedAction();
    }
    else
    {
        status_->setText("不支持的文件类型");
    }
}

void MainWindow::setPage(int index)
{
    stack_->setCurrentIndex(index);
    for (int i = 0; i < nav_.size(); ++i)
        nav_[i]->setChecked(i == index);
}

void MainWindow::toggleMaximize()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
    update(); // 重绘圆角（最大化时用直角）
}

void MainWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor panel(35, 39, 49, 175); // 主内容区：深色玻璃
    if (isMaximized())
    {
        p.fillRect(rect(), panel);
    }
    else
    {
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 12, 12);
        p.fillPath(path, panel);
    }
}

void MainWindow::mousePressEvent(QMouseEvent* e)
{
    // 无边框窗口：靠近四边/四角时交给系统缩放
    if (e->button() == Qt::LeftButton && !isMaximized())
    {
        const int m = 6;
        const QPoint p = e->pos();
        Qt::Edges edges;
        if (p.x() <= m) edges |= Qt::LeftEdge;
        if (p.x() >= width() - m) edges |= Qt::RightEdge;
        if (p.y() <= m) edges |= Qt::TopEdge;
        if (p.y() >= height() - m) edges |= Qt::BottomEdge;
        if (edges)
        {
            if (QWindow* w = window()->windowHandle())
                w->startSystemResize(edges);
            e->accept();
            return;
        }
    }
    QWidget::mousePressEvent(e);
}
