#include "src/ui/mainwindow.h"
#include "src/card/carditem.h"

#include <QApplication>
#include <QPixmap>
#include <QDebug>
#include <QDir>
#include <QDirIterator>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // ── 临时诊断,验证完删掉 ──
    auto diag = [](const QString &p) {
        QPixmap pm(p);
        qDebug() << p << pm.size();
    };
    diag(":/textures/images/Jokers.png");      // 应 1420×3040
    diag(":/textures/images/8BitDeck.png");    // 应 1846×760
    diag(":/textures/images/Enhancers.png");   // 应 994×950
    diag(":/textures/images/Tarots.png");      // 应 1420×760
    diag(":/textures/images/boosters.png");    // 应 568×1710
    diag(":/textures/images/BlindChips.png");  // 应 ~136×N（每个盲注 21 帧 ×68）
    diag(":/textures/images/chips.png");
    diag(":/textures/images/tags.png");

    qDebug() << "── qrc 实际内容 ──";
    QDirIterator it(":/textures", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) qDebug() << " ✓" << it.next();
    qDebug() << "── 磁盘实际内容 ──";
    QDir d("D:/QtProjects/BalatroQt/resources/images");
    for (const QString &f : d.entryList(QDir::Files))
        qDebug() << " ·" << f;

    CardItem::loadResources();
    JokerItem::loadResources();
    ConsumableItem::loadResources();
    MainWindow w;
    w.showFullScreen();
    return a.exec();
}
