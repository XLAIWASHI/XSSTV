#pragma once

#include <QWidget>

#include "xsstv/AudioBuffer.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QMediaPlayer;
class QAudioOutput;
class ImageLabel;

// 编码页：选图片 → 编码 → 播放/保存 WAV
// 只调用 Core 公开接口，不含任何编码算法
class EncodePage : public QWidget
{
    Q_OBJECT
public:
    explicit EncodePage(QWidget* parent = nullptr);

    void setInputImage(const QString& path); // 供拖拽等外部设置输入图片

signals:
    void status(const QString& text);

private slots:
    void chooseImage();
    void chooseOutput();
    void doEncode();
    void doPlay();
    void doStop();
    void doSaveWav();

private:
    void refreshPreviews();
    void setBusy(bool busy);

    QLineEdit* inEdit_ = nullptr;
    QLineEdit* outEdit_ = nullptr;
    ImageLabel* originalView_ = nullptr;
    ImageLabel* encodedView_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QPushButton* btnEncode_ = nullptr;
    QPushButton* btnPlay_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOut_ = nullptr;

    xsstv::AudioBuffer audio_;
    bool hasAudio_ = false;
    bool busy_ = false;
    QString wavPath_;
};
