#pragma once

#include <QVector>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;
class QMediaPlayer;
class QAudioOutput;
class QPushButton;
class QLabel;

// 资源管理页：搜索/筛选、缩略图、打开目录、预览/播放、另存为、重命名、删除
class ResourcePage : public QWidget
{
    Q_OBJECT
public:
    explicit ResourcePage(QWidget* parent = nullptr);

private slots:
    void refresh();
    void openResourceDir();
    void openContainingDir();
    void saveAs();
    void renameSelected();
    void removeSelected();
    void onDoubleClick(int row, int column);
    void playSelected();
    void stopPlayback();

private:
    int selectedRegIndex() const; // 表格行 → 登记表下标，无选中返回 -1
    void previewImage(const QString& path);
    void playWav(const QString& path);

    QComboBox* filter_ = nullptr;
    QLineEdit* search_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVector<int> rowToReg_; // 表格行 → 登记表下标

    QPushButton* btnPlay_ = nullptr;
    QPushButton* btnStop_ = nullptr;
    QLabel* playLabel_ = nullptr;

    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOut_ = nullptr;
    QString playingPath_;
};
