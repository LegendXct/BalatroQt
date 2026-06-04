#include "cardfloat.h"

#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QCoreApplication>

namespace {
struct Driver {
    QTimer *timer = nullptr;
    QElapsedTimer clock;
    QHash<QObject *, std::function<void(double)>> ticks;
};
Driver &driver()
{
    static Driver d;
    return d;
}
}

namespace CardFloat {

void add(QObject *owner, const std::function<void(double)> &tick)
{
    if (!owner) return;
    Driver &d = driver();
    if (!d.timer) {
        d.clock.start();
        d.timer = new QTimer(QCoreApplication::instance());
        d.timer->setTimerType(Qt::CoarseTimer);
        QObject::connect(d.timer, &QTimer::timeout, []() {
            Driver &dd = driver();
            const double t = dd.clock.elapsed() / 1000.0;
            // 拷一份当前回调列表再遍历——回调内部可能触发增删（如卡牌析构 remove 自己）。
            const auto ticks = dd.ticks;
            for (auto it = ticks.constBegin(); it != ticks.constEnd(); ++it)
                it.value()(t);
        });
        d.timer->start(33);   // ~30fps
    }
    d.ticks.insert(owner, tick);
}

void remove(QObject *owner)
{
    driver().ticks.remove(owner);
}

double elapsedSeconds()
{
    Driver &d = driver();
    return d.clock.isValid() ? d.clock.elapsed() / 1000.0 : 0.0;
}

}
