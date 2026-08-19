#pragma once
#include <QObject>
#include <QTime>
#include <QTimer>

// Surveille l'heure de debut du culte.
//
// Deux roles :
//   * fournir le decompte avant le debut, pour l'ecran d'accueil pendant que
//     l'assemblee s'installe ;
//   * declencher la premiere phase a l'heure dite, si l'option est active.
//
// Le declenchement ne concerne QUE la premiere phase. Les suivantes
// s'enchainent a la main : en pratique une phase deborde presque toujours, et
// une bascule a l'heure couperait quelqu'un en plein direct.
//
// Garde essentielle : le declenchement n'a lieu que si l'application a
// reellement vu l'heure arriver. Ouvrir le logiciel a 10h15 avec une heure de
// debut a 10h00 heritee de la semaine passee ne doit rien lancer.
class ServiceSchedule : public QObject
{
    Q_OBJECT
public:
    explicit ServiceSchedule(QObject *parent = nullptr);

    // Heure invalide : aucune surveillance.
    void setStartTime(const QTime& time);
    QTime startTime() const { return m_startTime; }

    void setAutoStartEnabled(bool enabled) { m_autoStart = enabled; }
    bool autoStartEnabled() const { return m_autoStart; }

    // Secondes avant le debut. Negatif si l'heure est passee,
    // -1 si aucune heure n'est definie.
    int secondsUntilStart() const;

    // Decompte formate "HH:mm:ss", vide si aucune heure n'est definie,
    // si l'heure est passee, ou si le culte a demarre.
    QString remainingText() const;

    // A appeler quand le programme demarre, par l'operateur ou
    // automatiquement : le decompte s'arrete et ne redeclenchera plus.
    void markStarted();

    // Remet la surveillance a zero, par exemple au chargement d'un programme.
    void reset();

    bool hasStarted() const { return m_started; }

signals:
    void tick(QString remaining);
    // Emis une seule fois, uniquement si l'application a vu le compte a
    // rebours passer par une valeur positive avant d'atteindre zero.
    void startTimeReached();

private:
    QTime m_startTime;
    QTimer m_timer;
    bool m_autoStart = false;
    bool m_started = false;
    bool m_wasPending = false;   // le decompte a ete observe comme positif
    QString m_lastText;

    void check();
};
