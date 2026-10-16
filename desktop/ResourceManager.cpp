#include "ResourceManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ResourceManager& ResourceManager::instance()
{
    static ResourceManager mgr;
    return mgr;
}

ResourceManager::ResourceManager()
{
    load();
}

QString ResourceManager::filePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath("resources.json");
}

void ResourceManager::load()
{
    items_.clear();
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    for (const QJsonValue& v : doc.array())
    {
        const QJsonObject o = v.toObject();
        items_.append({ o.value("path").toString(),
                        o.value("type").toString(),
                        o.value("created").toString() });
    }
}

void ResourceManager::save()
{
    QJsonArray arr;
    for (const Item& it : items_)
    {
        QJsonObject o;
        o.insert("path", it.path);
        o.insert("type", it.type);
        o.insert("created", it.created);
        arr.append(o);
    }
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(arr).toJson());
}

void ResourceManager::add(const QString& path, const QString& type)
{
    for (Item& it : items_)
    {
        if (it.path == path) // 已存在则更新时间
        {
            it.type = type;
            it.created = QDateTime::currentDateTime().toString(Qt::ISODate);
            save();
            emit changed();
            return;
        }
    }
    items_.append({ path, type, QDateTime::currentDateTime().toString(Qt::ISODate) });
    save();
    emit changed();
}

void ResourceManager::removeAt(int index)
{
    if (index < 0 || index >= items_.size())
        return;
    items_.removeAt(index);
    save();
    emit changed();
}

bool ResourceManager::renameAt(int index, const QString& newFileName)
{
    if (index < 0 || index >= items_.size() || newFileName.isEmpty())
        return false;

    const QFileInfo fi(items_[index].path);
    const QString newPath = QDir(fi.absolutePath()).filePath(newFileName);
    if (newPath == items_[index].path)
        return true;
    if (!QFile::rename(items_[index].path, newPath))
        return false;

    items_[index].path = newPath;
    save();
    emit changed();
    return true;
}
