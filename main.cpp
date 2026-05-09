#include "src/card/carditem.h"
#include "src/ui/mainwindow.h"

#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CardItem::loadResources();
    JokerItem::loadResources();
    MainWindow w;
    w.showFullScreen();
    return a.exec();
}
