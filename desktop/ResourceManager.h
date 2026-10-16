#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// 资源登记表：记录 XSSTV 成功生成的 WAV / 保存的图片。
// 用 JSON 持久化到用户数据目录，不引入数据库。
class ResourceManager : public QObject
{
    Q_OBJECT
public:
    struct Item
    {
        QString path;     // 文件绝对路径
        QString type;     // "wav" / "image"
        QString created;  // ISO 时间
    };

    static ResourceManager& instance();

    const QVector<Item>& items() const { return items_; }

    void add(const QString& path, const QString& type);
    void removeAt(int index);
    bool renameAt(int index, const QString& newFileName); // 重命名文件并更新登记

signals:
    void changed();

private:
    ResourceManager();
    QString filePath() const;
    void load();
    void save();

    QVector<Item> items_;
};
