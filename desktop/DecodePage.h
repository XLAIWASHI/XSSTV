#pragma once

#include <QWidget>

#include "xsstv/Image.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QMediaPlayer;
class QAudioOutput;
class QTimer;
class ImageLabel;

// 解码页：选 WAV → 解码 → 显示结果 / 保存图片
// 只调用 Core 公开接口，不含任何解码算法
class DecodePage : public QWidget
{
    Q_OBJECT
public:
    explicit DecodePage(QWidget* parent = nullptr);

    void setInputWav(const QString& path); // 供拖拽等外部设置输入 WAV

signals:
    void status(const QString& text);

private slots:
    void chooseWav();
    void chooseOutput();
    void doDecode();
    void doPlay();
    void doStop();
    void doSaveImage();

private:
    void showAudioInfo();
    void setBusy(bool busy);

    QLineEdit* wavEdit_ = nullptr;
    QLineEdit* outEdit_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QLabel* leftInfo_ = nullptr;
    QTimer* infoTimer_ = nullptr;
    ImageLabel* resultView_ = nullptr;
    QPushButton* btnDecode_ = nullptr;
    QPushButton* btnPlay_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QPushButton* btnSave_ = nullptr;

    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOut_ = nullptr;

    xsstv::Image result_;
    bool hasResult_ = false;
    bool busy_ = false;
    QString wavPath_;
};
