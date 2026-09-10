#include "mainwindow.h"
#include "database/database.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("VIAMED");
    QCoreApplication::setApplicationName("echo");

    if (!Database::init())
    {
        qDebug() << "DB init failed";
        return -1;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
