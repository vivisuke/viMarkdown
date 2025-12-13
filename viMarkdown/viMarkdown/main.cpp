#include "MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("VisualSoftLab");
    QCoreApplication::setApplicationName("viMarkdown");
    MainWindow window;
    window.show();
    return app.exec();
}
