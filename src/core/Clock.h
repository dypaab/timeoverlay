#pragma once
#include <QObject>
#include <QDateTime>
#include <QTimer>

// Horloge systeme. Les formats sont configurables : selon les eglises on
// veut "10:42:07", "10:42" ou "10 h 42", et la date en francais long.
// Syntaxe des formats : celle de QDateTime (HH mm ss, dddd d MMMM yyyy...).
class Clock : public QObject
{
    Q_OBJECT
public:
    explicit Clock(QString name, QObject *parent = nullptr);

    QString name() const { return m_name; }

    QString currentTime() const;
    QString currentDate() const;

    void setTimeFormat(const QString& format);
    void setDateFormat(const QString& format);
    QString timeFormat() const { return m_timeFormat; }
    QString dateFormat() const { return m_dateFormat; }

    static QString defaultTimeFormat() { return QStringLiteral("HH:mm:ss"); }
    static QString defaultDateFormat() { return QStringLiteral("dddd d MMMM yyyy"); }

    void start();
    void stop();
    bool isRunning() const { return m_timer.isActive(); }

signals:
    void tick(QString time, QString date);

private:
    QString m_name;
    QString m_timeFormat = defaultTimeFormat();
    QString m_dateFormat = defaultDateFormat();
    QTimer m_timer;
};
