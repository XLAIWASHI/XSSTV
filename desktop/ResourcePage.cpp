#include "ResourcePage.h"

#include <QAudioOutput>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "ImageLabel.h"
#include "ResourceManager.h"
#include "apppaths.h"

namespace
{
    // 生成缩略图（按比例缩到 max 像素内）
    QIcon thumbnail(const QString& path, int max)
    {
        QImageReader reader(path);
        const QSize sz = reader.size();
        if (!sz.isValid())
            return QIcon();
        if (sz.width() > max || sz.height() > max)
            reader.setScaledSize(sz.scaled(max, max, Qt::KeepAspectRatio));
        const QImage img = reader.read();
        return img.isNull() ? QIcon() : QIcon(QPixmap::fromImage(img));
    }
}

ResourcePage::ResourcePage(QWidget* parent)
    : QWidget(parent)
{
    auto* title = new QLabel("资源管理", this);
    title->setStyleSheet("font-weight: bold; font-size: 15px;");

    filter_ = new QComboBox(this);
    filter_->addItem("全部");
    filter_->addItem("WAV 音频");
    filter_->addItem("图片");

    search_ = new QLineEdit(this);
    search_->setPlaceholderText("搜索文件名/路径");
    search_->setAcceptDrops(false);

    auto* btnRefresh = new QPushButton("刷新", this);
    auto* btnResDir = new QPushButton("打开输出目录", this);
    auto* btnContDir = new QPushButton("打开所在目录", this);
    auto* btnSaveAs = new QPushButton("另存为", this);
    auto* btnRename = new QPushButton("重命名", this);
    auto* btnRemove = new QPushButton("删除", this);
    btnPlay_ = new QPushButton("播放/暂停", this);
    btnStop_ = new QPushButton("停止", this);
    playLabel_ = new QLabel("", this);
    playLabel_->setStyleSheet("color: rgba(255,255,255,150);");

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({ "文件名", "类型", "生成时间", "大小", "路径" });
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setIconSize(QSize(48, 48));
    table_->setAcceptDrops(false);
    table_->viewport()->setAcceptDrops(false);

    auto* top = new QHBoxLayout();
    top->addWidget(title);
    top->addSpacing(12);
    top->addWidget(new QLabel("筛选：", this));
    top->addWidget(filter_);
    top->addWidget(search_);
    top->addStretch();
    top->addWidget(btnRefresh);
    top->addWidget(btnResDir);

    auto* ops = new QHBoxLayout();
    ops->addWidget(btnContDir);
    ops->addWidget(btnSaveAs);
    ops->addWidget(btnRename);
    ops->addWidget(btnRemove);
    ops->addSpacing(12);
    ops->addWidget(btnPlay_);
    ops->addWidget(btnStop_);
    ops->addWidget(playLabel_);
    ops->addStretch();

    auto* main = new QVBoxLayout(this);
    main->addLayout(top);
    main->addWidget(table_, 1);
    main->addLayout(ops);

    player_ = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);

    connect(filter_, &QComboBox::currentTextChanged, this, &ResourcePage::refresh);
    connect(search_, &QLineEdit::textChanged, this, &ResourcePage::refresh);
    connect(btnRefresh, &QPushButton::clicked, this, &ResourcePage::refresh);
    connect(btnResDir, &QPushButton::clicked, this, &ResourcePage::openResourceDir);
    connect(btnContDir, &QPushButton::clicked, this, &ResourcePage::openContainingDir);
    connect(btnSaveAs, &QPushButton::clicked, this, &ResourcePage::saveAs);
    connect(btnRename, &QPushButton::clicked, this, &ResourcePage::renameSelected);
    connect(btnRemove, &QPushButton::clicked, this, &ResourcePage::removeSelected);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ResourcePage::onDoubleClick);
    connect(&ResourceManager::instance(), &ResourceManager::changed, this, &ResourcePage::refresh);
    connect(btnPlay_, &QPushButton::clicked, this, &ResourcePage::playSelected);
    connect(btnStop_, &QPushButton::clicked, this, &ResourcePage::stopPlayback);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState st) {
        if (st != QMediaPlayer::PlayingState && st != QMediaPlayer::PausedState)
            playLabel_->clear();
    });

    refresh();
}

void ResourcePage::refresh()
{
    table_->setRowCount(0);
    rowToReg_.clear();

    const QString f = filter_->currentText();
    const QString q = search_->text().trimmed();
    const auto& items = ResourceManager::instance().items();

    for (int i = 0; i < items.size(); ++i)
    {
        const auto& it = items[i];
        if (f == "WAV 音频" && it.type != "wav") continue;
        if (f == "图片" && it.type != "image") continue;
        if (!q.isEmpty() && !it.path.contains(q, Qt::CaseInsensitive)) continue;

        const QFileInfo fi(it.path);
        const int row = table_->rowCount();
        table_->insertRow(row);

        auto* nameItem = new QTableWidgetItem(fi.fileName());
        if (it.type == "image" && fi.exists())
            nameItem->setIcon(thumbnail(it.path, 48));
        table_->setItem(row, 0, nameItem);
        table_->setItem(row, 1, new QTableWidgetItem(it.type == "wav" ? "WAV 音频" : "图片"));
        table_->setItem(row, 2, new QTableWidgetItem(it.created));
        table_->setItem(row, 3, new QTableWidgetItem(
            fi.exists() ? QString::number(fi.size()) + " B" : "文件缺失"));
        table_->setItem(row, 4, new QTableWidgetItem(it.path));
        rowToReg_.append(i);
    }
    table_->resizeColumnsToContents();
}

int ResourcePage::selectedRegIndex() const
{
    const int row = table_->currentRow();
    if (row < 0 || row >= rowToReg_.size()) return -1;
    return rowToReg_[row];
}

void ResourcePage::openResourceDir()
{
    const QString dir = apppaths::outputDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ResourcePage::openContainingDir()
{
    const int idx = selectedRegIndex();
    if (idx < 0) { QMessageBox::information(this, "提示", "请先选中一项"); return; }
    const QString path = ResourceManager::instance().items()[idx].path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void ResourcePage::saveAs()
{
    const int idx = selectedRegIndex();
    if (idx < 0) { QMessageBox::information(this, "提示", "请先选中一项"); return; }

    const auto item = ResourceManager::instance().items()[idx];
    const QString dst = QFileDialog::getSaveFileName(this, "另存为", QFileInfo(item.path).fileName());
    if (dst.isEmpty()) return;

    if (QFile::exists(dst) && !QFile::remove(dst))
    {
        QMessageBox::warning(this, "错误", "目标已存在且无法覆盖");
        return;
    }
    if (!QFile::copy(item.path, dst))
    {
        QMessageBox::warning(this, "错误", "另存为失败");
        return;
    }
}

void ResourcePage::renameSelected()
{
    const int idx = selectedRegIndex();
    if (idx < 0) { QMessageBox::information(this, "提示", "请先选中一项"); return; }

    const auto item = ResourceManager::instance().items()[idx];
    bool ok = false;
    const QString newName = QInputDialog::getText(this, "重命名", "新文件名：",
        QLineEdit::Normal, QFileInfo(item.path).fileName(), &ok);
    if (!ok || newName.isEmpty()) return;

    if (!ResourceManager::instance().renameAt(idx, newName))
        QMessageBox::warning(this, "错误", "重命名失败（可能重名或文件缺失）");
}

void ResourcePage::removeSelected()
{
    const int idx = selectedRegIndex();
    if (idx < 0) { QMessageBox::information(this, "提示", "请先选中一项"); return; }

    const auto item = ResourceManager::instance().items()[idx];
    if (QMessageBox::question(this, "删除资源",
            "确定删除该资源（同时删除文件）？\n" + item.path) != QMessageBox::Yes)
        return;

    QFile::remove(item.path);                  // 删除已登记的文件
    ResourceManager::instance().removeAt(idx); // 移除登记（自动刷新）
}

void ResourcePage::onDoubleClick(int row, int)
{
    if (row < 0 || row >= rowToReg_.size()) return;
    const auto item = ResourceManager::instance().items()[rowToReg_[row]];
    if (item.type == "image") previewImage(item.path);
    else                      playWav(item.path);
}

void ResourcePage::previewImage(const QString& path)
{
    QImageReader reader(path);
    const QImage img = reader.read();
    if (img.isNull()) { QMessageBox::warning(this, "提示", "无法读取图片"); return; }

    QDialog dlg(this);
    dlg.setWindowTitle(QFileInfo(path).fileName());
    dlg.resize(640, 480);
    auto* view = new ImageLabel(&dlg);
    view->setImage(img);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(view);
    dlg.exec();
}

void ResourcePage::playWav(const QString& path)
{
    if (playingPath_ == path && player_->playbackState() == QMediaPlayer::PlayingState)
    {
        player_->pause();
        playLabel_->setText("已暂停：" + QFileInfo(path).fileName());
        return;
    }
    if (playingPath_ != path)
    {
        playingPath_ = path;
        player_->setSource(QUrl::fromLocalFile(path));
    }
    player_->play();
    playLabel_->setText("正在播放：" + QFileInfo(path).fileName());
}

void ResourcePage::playSelected()
{
    const int idx = selectedRegIndex();
    if (idx < 0) { QMessageBox::information(this, "提示", "请先选中一项"); return; }
    const auto item = ResourceManager::instance().items()[idx];
    if (item.type != "wav") { QMessageBox::information(this, "提示", "只有 WAV 音频可以播放"); return; }
    playWav(item.path);
}

void ResourcePage::stopPlayback()
{
    player_->stop();
    playLabel_->clear();
}
