#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// 默认路径：全部相对"程序所在目录"，方便打包分发（便携）
namespace apppaths
{
    inline QString appDir() { return QCoreApplication::applicationDirPath(); }

    inline QString defaultImage() { return QDir(appDir()).filePath("assets/test.png"); }
    inline QString outputDir()    { return QDir(appDir()).filePath("output"); }

    // 在 output 目录里找 robot36_NNNN.wav 的最大编号，返回下一个（补零 4 位）
    inline QString nextOutputWav()
    {
        QDir dir(outputDir());
        dir.mkpath(".");
        int maxN = 0;
        const QRegularExpression re("^robot36_(\\d+)\\.wav$");
        for (const QFileInfo& fi : dir.entryInfoList(QStringList() << "robot36_*.wav", QDir::Files))
        {
            const auto m = re.match(fi.fileName());
            if (m.hasMatch())
                maxN = qMax(maxN, m.captured(1).toInt());
        }
        return dir.filePath(QString("robot36_%1.wav").arg(maxN + 1, 4, 10, QChar('0')));
    }

    // 判断某个路径是不是"自动生成/编号"的名字：
    // robot36.wav（旧固定默认）或 robot36_NNNN.wav 都算自动名
    inline bool isAutoOutput(const QString& path)
    {
        return QRegularExpression("robot36(_\\d+)?\\.wav$").match(path).hasMatch();
    }

    // 解码输出的自动编号：decoded_NNNN.png
    inline QString nextOutputPng()
    {
        QDir dir(outputDir());
        dir.mkpath(".");
        int maxN = 0;
        const QRegularExpression re("^decoded_(\\d+)\\.png$");
        for (const QFileInfo& fi : dir.entryInfoList(QStringList() << "decoded_*.png", QDir::Files))
        {
            const auto m = re.match(fi.fileName());
            if (m.hasMatch())
                maxN = qMax(maxN, m.captured(1).toInt());
        }
        return dir.filePath(QString("decoded_%1.png").arg(maxN + 1, 4, 10, QChar('0')));
    }

    inline bool isAutoOutputPng(const QString& path)
    {
        return QRegularExpression("decoded(_\\d+)?\\.png$").match(path).hasMatch();
    }
}
