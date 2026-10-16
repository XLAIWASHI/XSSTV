#include "EncodePage.h"

#include <QAudioOutput>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "ImageLabel.h"
#include "ResourceManager.h"
#include "apppaths.h"
#include "imageconvert.h"
#include "xsstv/Image.h"
#include "xsstv/Robot36.h"
#include "xsstv/Wav.h"

namespace
{
    // 后台编码任务的结果
    struct EncodeResult
    {
        bool ok = false;
        QString msg;
        xsstv::AudioBuffer audio;
    };
}

EncodePage::EncodePage(QWidget* parent)
    : QWidget(parent)
{
    // 输入/输出路径
    inEdit_ = new QLineEdit(this);
    outEdit_ = new QLineEdit(this);
    inEdit_->setAcceptDrops(false); // 让文件拖拽交给主窗口处理
    outEdit_->setAcceptDrops(false);
    auto* btnIn = new QPushButton("浏览", this);
    auto* btnOut = new QPushButton("浏览", this);

    auto* pathLayout = new QGridLayout();
    pathLayout->addWidget(new QLabel("输入图片：", this), 0, 0);
    pathLayout->addWidget(inEdit_, 0, 1);
    pathLayout->addWidget(btnIn, 0, 2);
    pathLayout->addWidget(new QLabel("输出音频：", this), 1, 0);
    pathLayout->addWidget(outEdit_, 1, 1);
    pathLayout->addWidget(btnOut, 1, 2);

    // 预览：左原图 / 右编码输入图
    originalView_ = new ImageLabel(this);
    encodedView_ = new ImageLabel(this);
    auto* leftBox = new QGroupBox("原始图片", this);
    auto* rightBox = new QGroupBox("编码输入图 (320×240)", this);
    auto* l1 = new QVBoxLayout(leftBox);  l1->addWidget(originalView_);
    auto* l2 = new QVBoxLayout(rightBox); l2->addWidget(encodedView_);

    infoLabel_ = new QLabel("（未加载）", this);

    // 按钮
    auto* btnSelect = new QPushButton("选择图片", this);
    btnEncode_ = new QPushButton("开始编码", this);
    btnPlay_ = new QPushButton("播放音频", this);
    btnStop_ = new QPushButton("停止", this);
    btnSave_ = new QPushButton("保存 WAV", this);
    btnEncode_->setEnabled(false);
    btnPlay_->setEnabled(false);
    btnStop_->setEnabled(false);
    btnSave_->setEnabled(false);
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnSelect);
    btnLayout->addWidget(btnEncode_);
    btnLayout->addWidget(btnPlay_);
    btnLayout->addWidget(btnStop_);
    btnLayout->addWidget(btnSave_);

    auto* main = new QVBoxLayout(this);
    main->addLayout(pathLayout);
    main->addWidget(infoLabel_);
    auto* previews = new QHBoxLayout();
    previews->addWidget(leftBox, 1);
    previews->addWidget(rightBox, 2);
    main->addLayout(previews, 1);
    main->addLayout(btnLayout);

    // 播放器
    player_ = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);

    connect(btnIn, &QPushButton::clicked, this, &EncodePage::chooseImage);
    connect(btnSelect, &QPushButton::clicked, this, &EncodePage::chooseImage);
    connect(btnOut, &QPushButton::clicked, this, &EncodePage::chooseOutput);
    connect(btnEncode_, &QPushButton::clicked, this, &EncodePage::doEncode);
    connect(btnPlay_, &QPushButton::clicked, this, &EncodePage::doPlay);
    connect(btnStop_, &QPushButton::clicked, this, &EncodePage::doStop);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState st) {
        if (st == QMediaPlayer::StoppedState) { btnPlay_->setText("播放音频"); btnStop_->setEnabled(false); }
    });
    connect(btnSave_, &QPushButton::clicked, this, &EncodePage::doSaveWav);
    connect(inEdit_, &QLineEdit::textChanged, this, &EncodePage::refreshPreviews);

    // 记住用户改过的路径
    connect(inEdit_, &QLineEdit::textChanged, this, [](const QString& t) { QSettings().setValue("paths/inputImage", t); });
    connect(outEdit_, &QLineEdit::textChanged, this, [](const QString& t) { QSettings().setValue("paths/outputWav", t); });

    // 载入上次使用的路径；输出用自动编号（没设过、或上次就是自动编号时，取下一个）
    QSettings s;
    inEdit_->setText(s.value("paths/inputImage", apppaths::defaultImage()).toString());
    QString out = s.value("paths/outputWav").toString();
    if (out.isEmpty() || apppaths::isAutoOutput(out))
        out = apppaths::nextOutputWav();
    outEdit_->setText(out);
}

void EncodePage::setInputImage(const QString& path)
{
    inEdit_->setText(path); // 触发预览刷新与设置记忆
}

void EncodePage::chooseImage()
{
    const QString p = QFileDialog::getOpenFileName(
        this, "选择图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp)");
    if (!p.isEmpty())
        inEdit_->setText(p);
}

void EncodePage::chooseOutput()
{
    const QString p = QFileDialog::getSaveFileName(
        this, "输出 WAV", "robot36.wav", "WAV (*.wav)");
    if (!p.isEmpty())
        outEdit_->setText(p);
}

void EncodePage::refreshPreviews()
{
    const std::string path = inEdit_->text().toUtf8().toStdString();
    if (path.empty())
    {
        originalView_->clearImage();
        encodedView_->clearImage();
        infoLabel_->setText("（未加载）");
        btnEncode_->setEnabled(false);
        return;
    }

    xsstv::Image img;
    if (!xsstv::load_image(path, img))
    {
        originalView_->clearImage();
        encodedView_->clearImage();
        infoLabel_->setText("读取失败：" + inEdit_->text());
        btnEncode_->setEnabled(false);
        return;
    }

    originalView_->setImage(toQImage(img));

    // 编码输入图 = Core 编码时实际使用的最终图像（同一个 fit_image）
    xsstv::Image fit = xsstv::fit_image(img, 320, 240);
    encodedView_->setImage(toQImage(fit));

    infoLabel_->setText(QString("原图 %1×%2　→　编码输入 320×240")
                            .arg(img.width).arg(img.height));
    btnEncode_->setEnabled(true);
}

void EncodePage::setBusy(bool busy)
{
    busy_ = busy;
    btnEncode_->setEnabled(!busy);
    if (busy)
    {
        btnPlay_->setEnabled(false);
        btnStop_->setEnabled(false);
        btnSave_->setEnabled(false);
    }
}

void EncodePage::doEncode()
{
    if (busy_) return;
    if (outEdit_->text().isEmpty())
    {
        emit status("请先指定输出 WAV 路径");
        return;
    }

    const QString in = inEdit_->text();
    const QString out = outEdit_->text();
    setBusy(true);
    emit status("正在编码……");

    // 后台线程执行，结果经信号槽回到主线程
    auto* watcher = new QFutureWatcher<EncodeResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, out]() {
        const EncodeResult r = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (!r.ok)
        {
            emit status("错误：" + r.msg);
            btnPlay_->setEnabled(hasAudio_);
            btnSave_->setEnabled(hasAudio_);
            return;
        }
        audio_ = r.audio;
        hasAudio_ = true;
        wavPath_ = out;
        ResourceManager::instance().add(wavPath_, "wav");
        btnPlay_->setEnabled(true);
        btnSave_->setEnabled(true);
        emit status("编码完成：" + wavPath_);

        // 若用的是自动编号，把输出框推进到下一个编号（像 CLI 那样往后排）
        if (apppaths::isAutoOutput(out))
            outEdit_->setText(apppaths::nextOutputWav());
    });
    watcher->setFuture(QtConcurrent::run([in, out]() -> EncodeResult {
        EncodeResult r;
        xsstv::Image img;
        if (!xsstv::load_image(in.toUtf8().toStdString(), img)) { r.msg = "读图失败"; return r; }
        xsstv::Robot36 enc(48000);
        r.audio = enc.encode(img);
        if (!xsstv::write_wav(out.toUtf8().toStdString(), r.audio)) { r.msg = "写 WAV 失败"; return r; }
        r.ok = true;
        return r;
    }));
}

void EncodePage::doPlay()
{
    if (!hasAudio_) return;

    if (player_->playbackState() == QMediaPlayer::PlayingState)
    {
        player_->pause();
        btnPlay_->setText("继续播放");
        emit status("已暂停");
        return;
    }

    if (player_->source() != QUrl::fromLocalFile(wavPath_))
        player_->setSource(QUrl::fromLocalFile(wavPath_));
    player_->play();
    btnPlay_->setText("暂停");
    btnStop_->setEnabled(true);
    emit status("正在播放：" + wavPath_);
}

void EncodePage::doStop()
{
    player_->stop();
    btnPlay_->setText("播放音频");
    btnStop_->setEnabled(false);
    emit status("已停止播放");
}

void EncodePage::doSaveWav()
{
    if (!hasAudio_) return;
    const QString p = QFileDialog::getSaveFileName(
        this, "另存为 WAV", "robot36.wav", "WAV (*.wav)");
    if (p.isEmpty()) return;
    if (!xsstv::write_wav(p.toUtf8().toStdString(), audio_))
    {
        emit status("错误：保存 WAV 失败");
        return;
    }
    emit status("已保存：" + p);
}
