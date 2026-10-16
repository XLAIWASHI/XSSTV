// XSSTV 桌面端入口
#include <QApplication>

#include "MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("XSSTV");
    QApplication::setApplicationName("XSSTV");

    MainWindow w;
    w.show();

    return app.exec();
}
