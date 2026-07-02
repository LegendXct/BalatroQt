#ifndef BALATROMOTION_H
#define BALATROMOTION_H

#include <QPointF>
#include <QtGlobal>

namespace BalatroMotion {

struct AmbientTiltState {
    QPointF normalizedMouse;
    QPointF degrees;
    double amount = 0.0;
};

quint64 nextCardLikeId();
AmbientTiltState ambientTiltState(quint64 id, double seconds, double ambientTilt);
QPointF ambientTiltDegrees(quint64 id, double seconds, double ambientTilt);

} // namespace BalatroMotion

#endif // BALATROMOTION_H
