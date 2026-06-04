#ifndef CARDFLOAT_H
#define CARDFLOAT_H

#include <QObject>
#include <functional>

// 全局"环境漂浮"驱动：~30fps 统一滴答，回调带经过秒数。复刻原版 card.lua:16/4379 的
// ambient_tilt——每张牌（商店/槽位/卡包/手牌/牌库）都按各自相位缓慢做立体浮动倾斜。
// 用一个共享 timer 而不是每张牌各开一只，避免几十只 QTimer 抢主线程。
namespace CardFloat {
    // owner 作为去重 key；tick(经过秒数) 每帧调用。
    void add(QObject *owner, const std::function<void(double)> &tick);
    void remove(QObject *owner);
    double elapsedSeconds();
}

#endif // CARDFLOAT_H
