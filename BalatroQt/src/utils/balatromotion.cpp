#include "balatromotion.h"

#include <QAtomicInteger>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
QAtomicInteger<quint64> sNextCardLikeId = 0;
}

namespace BalatroMotion {

quint64 nextCardLikeId()
{
    return sNextCardLikeId.fetchAndAddRelaxed(1) + 1;
}

QPointF ambientTiltDegrees(quint64 id, double seconds, double ambientTilt)
{
    return ambientTiltState(id, seconds, ambientTilt).degrees;
}

AmbientTiltState ambientTiltState(quint64 id, double seconds, double ambientTilt)
{
    const double cardId = static_cast<double>(std::max<quint64>(1, id));
    const double tiltAngle = seconds * (1.56 + std::fmod(cardId / 1.14212, 1.0))
                           + cardId / 1.35122;

    // Original card.lua drives the dissolve vertex shader by moving a fake mouse
    // inside the card bounds:
    //   mx = (0.5 + 0.5*ambient*cos(angle))*w + x
    //   my = (0.5 + 0.5*ambient*sin(angle))*h + y
    // Current Qt cards still use a QTransform tilt, so this converts the same
    // normalized fake-mouse path into the existing degree-based transform.
    const double nx = 0.5 * ambientTilt * std::cos(tiltAngle);
    const double ny = 0.5 * ambientTilt * std::sin(tiltAngle);
    const double amount = ambientTilt * (0.5 + std::cos(tiltAngle)) * 0.3;

    AmbientTiltState state;
    state.normalizedMouse = QPointF(0.5 + nx, 0.5 + ny);
    state.amount = std::max(0.0, amount);

    const double degreeScale = 6.0 * (state.amount / std::max(0.001, ambientTilt * 0.45));
    const double limit = std::max(0.7, ambientTilt * 3.5);
    state.degrees = QPointF(qBound(-limit, nx * degreeScale, limit),
                            qBound(-limit, ny * degreeScale, limit));
    return state;
}

} // namespace BalatroMotion
