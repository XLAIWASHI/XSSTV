#include "DecodePage.h"

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
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "ImageLabel.h"
#include "ResourceManager.h"
#include "apppaths.h"
#include "imageconvert.h"
#include "xsstv/Robot36.h"
#include "xsstv/Wav.h"

namespace
{
    // 后台解码任务的结果
    struct DecodeResult
    {
        bool ok = false;
        QString msg;
        xsstv::Image image;
    };
}

DecodePage::DecodePage(QWidget* parent)
    : QWidget(parent)
{
    wavEdit_ = new QLineEdit(this);
    outEdit_ = new QLineEdit(this);
    wavEdit_->setAcceptDrops(false); // 让文件拖拽交给主窗口处理
    outEdit_->setAcceptDrops(false);
    auto* btnBrowse = new QPushButton("浏览", this);
    auto* btnOutBrowse = new QPushButton("浏览", this);

    auto* pathLayout = new QGridLayout();
    pathLayout->addWidget(new QLabel("输入 WAV：", this), 0, 0);
    pathLayout->addWidget(wavEdit_, 0, 1);
    pathLayout->addWidget(btnBrowse, 0, 2);
    pathLayout->addWidget(new QLabel("输出图片：", this), 1, 0);
    pathLayout->addWidget(outEdit_, 1, 1);
    pathLayout->addWidget(btnOutBrowse, 1, 2);

    infoLabel_ = new QLabel("（未选择）", this);

    // 左：音频信息；右：解码结果
    auto* leftBox = new QGroupBox("音频信息", this);
    leftInfo_ = new QLabel("选择 WAV 后显示采样率、时长、采样点数", this);
    leftInfo_->setWordWrap(true);
    leftInfo_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto* l1 = new QVBoxLayout(leftBox);
    l1->addWidget(leftInfo_);

    auto* rightBox = new QGroupBox("解码结果", this);
    resultView_ = new ImageLabel(this);
    auto* l2 = new QVBoxLayout(rightBox);
    l2->addWidget(resultView_);

    auto* btnSelect = new QPushButton("选择 WAV", this);
    btnDecode_ = new QPushButton("开始解码", this);
    btnPlay_ = new QPushButton("播放音频", this);
    btnStop_ = new QPushButton("停止", this);
    btnSave_ = new QPushButton("保存图片", this);
    btnDecode_->setEnabled(false);
    btnPlay_->setEnabled(false);
    btnStop_->setEnabled(false);
    btnSave_->setEnabled(false);
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnSelect);
    btnLayout->addWidget(btnDecode_);
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

    player_ = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);

    connect(btnBrowse, &QPushButton::clicked, this, &DecodePage::chooseWav);
    connect(btnOutBrowse, &QPushButton::clicked, this, &DecodePage::chooseOutput);
    connect(btnSelect, &QPushButton::clicked, this, &DecodePage::chooseWav);
    connect(btnDecode_, &QPushButton::clicked, this, &DecodePage::doDecode);
    connect(btnPlay_, &QPushButton::clicked, this, &DecodePage::doPlay);
    connect(btnStop_, &QPushButton::clicked, this, &DecodePage::doStop);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState st) {
        if (st == QMediaPlayer::StoppedState) { btnPlay_->setText("播放音频"); btnStop_->setEnabled(false); }
    });
    connect(btnSave_, &QPushButton::clicked, this, &DecodePage::doSaveImage);

    // 输入变化后延迟再读 WAV（防抖，避免逐字输入时反复读整个文件）
    infoTimer_ = new QTimer(this);
    infoTimer_->setSingleShot(true);
    infoTimer_->setInterval(250);
    connect(infoTimer_, &QTimer::timeout, this, &DecodePage::showAudioInfo);
    connect(wavEdit_, &QLineEdit::textChanged, this, [this] { infoTimer_->start(); });

    // 记住并恢复路径
    connect(wavEdit_, &QLineEdit::textChanged, this, [](const QString& t) { QSettings().setValue("paths/inputWav", t); });
    connect(outEdit_, &QLineEdit::textChanged, this, [](const QString& t) { QSettings().setValue("paths/outputImage", t); });
    QSettings s;
    wavEdit_->setText(s.value("paths/inputWav").toString());
    QString out = s.value("paths/outputImage").toString();
    if (out.isEmpty() || apppaths::isAutoOutputPng(out))
        out = apppaths::nextOutputPng();
    outEdit_->setText(out);
}

void DecodePage::setInputWav(const QString& path)
{
    wavEdit_->setText(path); // 触发音频信息刷新与设置记忆
}

void DecodePage::chooseWav()
{
    const QString p = QFileDialog::getOpenFileName(
        this, "选择 WAV", QString(), "WAV (*.wav)");
    if (!p.isEmpty())
        wavEdit_->setText(p);
}

void DecodePage::showAudioInfo()
{
    // 清空旧结果，避免误导
    resultView_->clearImage();
    hasResult_ = false;
    btnSave_->setEnabled(false);
    btnDecode_->setEnabled(false);
    btnPlay_->setEnabled(false);

    const std::string path = wavEdit_->text().toUtf8().toStdString();
    if (path.empty())
    {
        infoLabel_->setText("（未选择）");
        leftInfo_->setText("选择 WAV 后显示采样率、时长、采样点数");
        return;
    }

    xsstv::AudioBuffer audio;
    if (!xsstv::read_wav(path, audio))
    {
        infoLabel_->setText("读取失败：" + wavEdit_->text());
        leftInfo_->setText("读取失败：" + wavEdit_->text());
        return;
    }

    const double seconds = (audio.sample_rate > 0)
        ? double(audio.mono.size()) / audio.sample_rate : 0.0;
    const QString info = QString("采样率：%1 Hz\n采样点：%2\n时长：%3 秒")
                             .arg(audio.sample_rate)
                             .arg(audio.mono.size())
                             .arg(seconds, 0, 'f', 2);
    infoLabel_->setText(QString("采样率 %1 Hz　采样点 %2　时长 %3 秒")
                            .arg(audio.sample_rate).arg(audio.mono.size())
                            .arg(seconds, 0, 'f', 2));
    leftInfo_->setText(info);

    btnDecode_->setEnabled(true);
    btnPlay_->setEnabled(true);
    wavPath_ = wavEdit_->text();
}

void DecodePage::setBusy(bool busy)
{
    busy_ = busy;
    btnDecode_->setEnabled(!busy);
    if (busy)
        btnSave_->setEnabled(false);
}

void DecodePage::chooseOutput()
{
    const QString p = QFileDialog::getSaveFileName(
        this, "输出图片", "decoded.png", "PNG (*.png)");
    if (!p.isEmpty())
        outEdit_->setText(p);
}

void DecodePage::doDecode()
{
    if (busy_) return;
    const QString wav = wavEdit_->text();
    setBusy(true);
    emit status("正在解码……");

    // 后台线程执行，结果经信号槽回到主线程
    auto* watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const DecodeResult r = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (!r.ok)
        {
            emit status("错误：" + r.msg);
            btnSave_->setEnabled(hasResult_);
            return;
        }
        result_ = r.image;
        resultView_->setImage(toQImage(result_));
        hasResult_ = true;
        btnSave_->setEnabled(true);
        emit status("解码完成");
    });
    watcher->setFuture(QtConcurrent::run([wav]() -> DecodeResult {
        DecodeResult r;
        xsstv::AudioBuffer audio;
        if (!xsstv::read_wav(wav.toUtf8().toStdString(), audio)) { r.msg = "读 WAV 失败"; return r; }
        xsstv::Robot36 dec(48000);
        r.image = dec.decode(audio);
        r.ok = true;
        return r;
    }));
}

void DecodePage::doPlay()
{
    if (wavPath_.isEmpty()) return;

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

void DecodePage::doStop()
{
    player_->stop();
    btnPlay_->setText("播放音频");
    btnStop_->setEnabled(false);
    emit status("已停止播放");
}

void DecodePage::doSaveImage()
{
    if (!hasResult_) return;

    QString out = outEdit_->text();
    if (out.isEmpty())
    {
        out = QFileDialog::getSaveFileName(this, "保存图片", "decoded.png", "PNG (*.png)");
        if (out.isEmpty()) return;
    }

    if (!xsstv::save_image(out.toUtf8().toStdString(), result_))
    {
        emit status("错误：保存图片失败");
        return;
    }
    ResourceManager::instance().add(out, "image"); // 登记为资源
    emit status("已保存：" + out);

    // 若用的是自动编号，推进到下一个编号
    if (apppaths::isAutoOutputPng(out))
        outEdit_->setText(apppaths::nextOutputPng());
}
