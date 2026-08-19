#include "Clock.h"
#include <QLocale>

Clock::Clock(QString name, QObject *parent)
    : QObject(parent), m_name(std::move(name))
{
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        emit tick(currentTime(), currentDate());
    });

    // 200 ms plutot qu'une seconde : un timer a 1 Hz derive par rapport a la
    // seconde systeme, et l'horloge peut alors sauter une seconde a l'ecran.
    // Le moteur de sortie n'ecrit que ce qui change, donc cela ne cree pas
    // d'ecritures supplementaires.
    m_timer.setInterval(200);
}

QString Clock::currentTime() const
{
    return QLocale::system().toString(QDateTime::currentDateTime(), m_timeFormat);
}

QString Clock::currentDate() const
{
    return QLocale::system().toString(QDateTime::currentDateTime(), m_dateFormat);
}

void Clock::setTimeFormat(const QString& format)
{
    m_timeFormat = format.isEmpty() ? defaultTimeFormat() : format;
}

void Clock::setDateFormat(const QString& format)
{
    m_dateFormat = format.isEmpty() ? defaultDateFormat() : format;
}

void Clock::start()
{
    m_timer.start();
    emit tick(currentTime(), currentDate());
}

void Clock::stop()
{
    m_timer.stop();
}
