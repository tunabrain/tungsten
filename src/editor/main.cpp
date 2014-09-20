#include <embree/include/embree.h>
#include <QDesktopWidget>
#include <QApplication>
#include <QGLWidget>

#include "thread/ThreadUtils.hpp"

#include "RenderWindow.hpp"
#include "MainWindow.hpp"

using namespace Tungsten;

int main(int argc, char *argv[])
{
    int threadCount = max(ThreadUtils::idealThreadCount() - 1, 1u);
    ThreadUtils::startThreads(threadCount);

    embree::rtcInit();
    embree::rtcStartThreads(threadCount);

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

    try {
        return app.exec();
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
