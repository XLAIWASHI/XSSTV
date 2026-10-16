#include "TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

TitleBar::TitleBar(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setObjectName("titleBar");

    auto* icon = new QLabel("*", this);
    icon->setObjectName("appIcon");

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setObjectName("appTitle");

    auto* btnMin = new QToolButton(this);
    auto* btnMax = new QToolButton(this);
    auto* btnClose = new QToolButton(this);
    btnMin->setText("—");
    btnMax->setText("□");
    btnClose->setText("×");
    btnClose->setObjectName("closeButton");
    for (auto* b : { btnMin, btnMax, btnClose })
    {
        b->setFixedSize(40, 28);
        b->setCursor(Qt::ArrowCursor);
    }

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(6);
    layout->addWidget(icon);
    layout->addWidget(titleLabel_);
    layout->addStretch();
    layout->addWidget(btnMin);
    layout->addWidget(btnMax);
    layout->addWidget(btnClose);

    connect(btnMin, &QToolButton::clicked, this, &TitleBar::minimizeClicked);
    connect(btnMax, &QToolButton::clicked, this, &TitleBar::maximizeClicked);
    connect(btnClose, &QToolButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        if (QWindow* w = window()->windowHandle())
            w->startSystemMove(); // 交给系统拖动（含贴边、多显示器）
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        emit maximizeClicked();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}
