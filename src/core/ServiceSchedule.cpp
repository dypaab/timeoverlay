#include "ServiceSchedule.h"
#include <QDateTime>

ServiceSchedule::ServiceSchedule(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &ServiceSchedule::check);
    m_timer.start();
}

void ServiceSchedule::setStartTime(const QTime& time)
{
    m_startTime = time;
    m_started = false;
    m_wasPending = false;
    m_lastText.clear();
    check();
}

void ServiceSchedule::reset()
{
    m_started = false;
    m_wasPending = false;
    m_lastText.clear();
    check();
}

void ServiceSchedule::markStarted()
{
    m_started = true;
    m_wasPending = false;
    if (!m_lastText.isEmpty()) {
        m_lastText.clear();
        emit tick(QString());
    }
}

int ServiceSchedule::secondsUntilStart() const
{
    if (!m_startTime.isValid()) return -1;
    const QTime now = QTime::currentTime();
    return int(now.secsTo(m_startTime));
}

QString ServiceSchedule::remainingText() const
{
    if (m_started || !m_startTime.isValid()) return QString();

    const int remaining = secondsUntilStart();
    if (remaining <= 0) return QString();

    return QStringLiteral("%1:%2:%3")
        .arg(remaining / 3600, 2, 10, QChar('0'))
        .arg((remaining % 3600) / 60, 2, 10, QChar('0'))
        .arg(remaining % 60, 2, 10, QChar('0'));
}

void ServiceSchedule::check()
{
    if (!m_startTime.isValid() || m_started) {
        if (!m_lastText.isEmpty()) {
            m_lastText.clear();
            emit tick(QString());
        }
        return;
    }

    const int remaining = secondsUntilStart();

    if (remaining > 0) {
        // On a vu le decompte courir : le declenchement devient legitime.
        m_wasPending = true;
    } else if (m_wasPending) {
        // L'heure vient d'arriver sous nos yeux.
        m_wasPending = false;
        m_started = true;
        if (!m_lastText.isEmpty()) {
            m_lastText.clear();
            emit tick(QString());
        }
        if (m_autoStart) {
            emit startTimeReached();
        }
        return;
    }

    const QString text = remainingText();
    if (text != m_lastText) {
        m_lastText = text;
        emit tick(text);
    }
}
