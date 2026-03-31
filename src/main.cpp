#include <QApplication>
#include <QIcon>

#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/suzhouMetroLogo.ico"));

    MainWindow window;
    window.show();

    return app.exec();
}
