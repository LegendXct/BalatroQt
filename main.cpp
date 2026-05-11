#include "src/card/carditem.h"
#include "src/ui/mainwindow.h"

#include <QApplication>
#include <QPixmap>
#include <QDebug>
#include <QDir>
#include <QDirIterator>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    CardItem::loadResources();
    JokerItem::loadResources();
    ConsumableItem::loadResources();
    MainWindow w;
    w.showFullScreen();
    return a.exec();
}
