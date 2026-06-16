#include "camclaw/qt/CamMainWindow.h"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    camclaw::CamMainWindow window;
    window.show();
    return app.exec();
}
