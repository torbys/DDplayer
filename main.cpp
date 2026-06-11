#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用信息（用于QSettings保存路径）
    a.setOrganizationName("YourCompany");
    a.setApplicationName("YourVideoPlayer");

    MainWindow w;
    w.show();
    return a.exec();
}
