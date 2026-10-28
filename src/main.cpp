#include <QApplication>
#include <QIcon>
#include "views/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SKINPEELER");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("Voronoi Diagram Research Center, Hanyang University, Korea");
    app.setWindowIcon(QIcon(":/icon.png"));

    MainWindow w;
    w.show();

    return app.exec();
}
