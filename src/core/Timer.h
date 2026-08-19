#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

// Minuteur decroissant qui bascule en depassement une fois arrive a zero.
//
// Le temps est calcule a partir d'une horloge monotone (QElapsedTimer), pas
// en comptant les impulsions du QTimer. C'est essentiel : Qt ne garantit pas
// qu'un timer se declenche exactement toutes les secondes, et les impulsions
// manquees quand la machine est chargee etaient purement et simplement
// perdues. Sur une predication de 45 minutes, l'ancienne version pouvait
// deriver de plusieurs secondes -- inacceptable quand c'est justement le
// temps affiche a l'orateur.
class Timer : public QObject
{
    Q_OBJECT
public:
    explicit Timer(QString name, int seconds, QObject *parent = nullptr);

    enum class State {
        STOPPED,     // pas demarre, ou remis a zero
        RUNNING,     // decompte en cours
        PAUSED,      // en pause, reprise possible
        FINISHED,    // arrive a zero, depassement desactive
        OVERTIME     // arrive a zero, compte maintenant a la hausse
    };

    // Demarre, ou reprend si le minuteur est en pause.
    // Ne repart jamais du debut : en direct, un operateur qui appuie sur
    // Demarrer apres une pause veut reprendre, pas tout perdre.
    void start();

    // Repart explicitement du debut.
    void restart();

    void pause();
    void reset();

    // Ajuste la duree en cours de route. Indispensable en regie :
    // "laisse-lui deux minutes de plus" sans casser le decompte.
    void addSeconds(int delta);
    void setDuration(int seconds);

    // Si desactive, le minuteur s'arrete a zero (etat FINISHED) au lieu de
    // basculer en depassement.
    void setOvertimeEnabled(bool enabled) { m_overtimeEnabled = enabled; }
    bool overtimeEnabled() const { return m_overtimeEnabled; }

    QString name() const { return m_name; }
    State state() const { return m_state; }
    int durationSeconds() const { return int(m_durationMs / 1000); }

    // Temps reellement ecoule depuis le demarrage, depassement compris.
    int elapsedSeconds() const;
    // Temps restant, plancher a zero.
    int remainingSeconds() const;
    // Secondes ecoulees au-dela de zero, zero tant qu'on ne depasse pas.
    int overtimeSeconds() const;

    QString countdown() const;   // "00:04:31", "00:00:00" une fois a zero
    QString overtime() const;    // "+00:01:22", vide tant qu'on ne depasse pas
    QString countup() const;     // temps ecoule depuis le demarrage

    static QString format(qint64 totalSeconds);

    // Analyse une duree saisie a la main. Accepte "25" (minutes), "05:30"
    // (min:sec) et "01:15:00" (h:min:sec) -- les trois notations circulent
    // quand on prepare un deroule ou qu'on lance un minuteur a la volee.
    // Renvoie false si le texte est inexploitable.
    static bool parseDuration(const QString& text, int* outSeconds);

signals:
    void tick(QString countdown, QString overtime, State state);
    void finished();          // emis une seule fois, au passage a zero
    void overtimeStarted();   // emis une seule fois, si le depassement est actif
    void stateChanged(State state);

private:
    QString m_name;
    qint64 m_durationMs;
    qint64 m_accumulatedMs = 0;   // temps ecoule cumule hors periodes de pause
    QElapsedTimer m_elapsed;      // mesure la periode en cours
    QTimer m_ticker;
    State m_state = State::STOPPED;
    bool m_overtimeEnabled = true;
    bool m_finishedEmitted = false;

    qint64 elapsedMs() const;
    void setState(State s);
    void onTick();
};
