#include <embree/include/embree.h>
#include <QDesktopWidget>
#include <QApplication>
#include <QGLWidget>

#include "RenderWindow.hpp"
#include "MainWindow.hpp"

using namespace Tungsten;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QDesktopWidget desktop;
    QRect windowSize(desktop.screenGeometry(desktop.primaryScreen()));
    windowSize.adjust(100, 100, -100, -100);

    MainWindow mainWindow;
    mainWindow.setWindowTitle("Tungsten Scene Editor");
    mainWindow.setGeometry(windowSize);

    mainWindow.show();

    if (argc > 1)
        mainWindow.openScene(QString(argv[1]));

    embree::rtcInit();
    //embree::rtcSetVerbose(1);
    embree::rtcStartThreads(8);

    try {
        return app.exec();
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
