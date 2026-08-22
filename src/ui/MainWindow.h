#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include "../core/Clock.h"
#include "../core/Timer.h"
#include "../core/Profile.h"
#include "../core/PhaseManager.h"
#include "../core/Alarm.h"
#include "../core/ColorManager.h"
#include "../core/OutputEngine.h"
#include "../core/MessageCenter.h"
#include "../core/ServiceSchedule.h"
#include "../core/ProgrammeLibrary.h"

class OverlayWindow;
class MessagePanel;
class OutputPathsDialog;
class HelpDialog;
class QAction;
class QLineEdit;

// Console de regie.
//
// Point d'attention historique : l'horloge emet un premier tick de maniere
// synchrone des l'appel a start(). Tout ce que le slot d'horloge utilise doit
// donc exister AVANT ce demarrage. L'ancienne version demarrait l'horloge
// avant de creer l'overlay et se terminait par un dereferencement de pointeur
// nul, soit un plantage systematique au lancement.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Charge un programme depuis un fichier, sans passer par la boite de
    // dialogue. Permet d'associer les .timerproject a l'application, ou de
    // creer un raccourci qui ouvre directement le programme du dimanche.
    // Renvoie false et affiche l'erreur si le fichier est inutilisable.
    bool openProgrammeFile(const QString& path);

    // Propose de reprendre le culte la ou il en etait, si l'application s'est
    // fermee en pleine phase il y a peu. Renvoie true si la reprise a eu lieu.
    // Appelee depuis main() apres show(), pour que la question s'affiche
    // au-dessus d'une fenetre deja visible.
    bool proposeSessionRestore();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onStartPause();
    void onNextPhase();
    void onPreviousPhase();
    void onAddMinute();
    void onRemoveMinute();
    void onResetProgramme();

    void onNewProgramme();
    void onLoadProgramme();
    void onOpenLibrary();
    void onSaveProgramme();
    void onSaveProgrammeAs();
    void onEditProgramme();
    void onExportSession();

    void onToggleOverlay();
    void onOpenSettings();
    void onOpenOutputFolder();
    void onShowHelp();
    void onHelpAction(const QString& id);
    void onAbout();

    void onQuickStartPause();
    void onQuickReset();
    void onQuickTick(QString countdown, QString overtime, Timer::State state);
    void onQuickFinished();
    void onShowOutputPaths();

    void onClockTick(QString time, QString date);
    void onPhaseTick(QString countdown, QString overtime, Timer::State state);
    void onPhaseChanged(int index, QString name, int durationSeconds);
    void onPhaseFinished(int index, QString name);
    void onAllPhasesFinished();
    void onScheduleTick(QString remaining);
    void onStartTimeReached();
    void onWriteFailed(const QString& path, const QString& reason);

private:
    void buildUi();
    QWidget* buildQuickTimerBox(QWidget* parent);
    void buildMenus();
    void buildToolbar();
    void buildMessageDock();
    void wireSignals();

    // Qui alimente countdown.txt, countup.txt et depassement.txt.
    //
    // Le minuteur rapide ecrit dans les MEMES fichiers que le programme, pour
    // qu'OBS n'ait pas a etre reconfigure. Les deux ne doivent donc jamais
    // tourner ensemble : ce serait exactement l'ecrasement mutuel qui rendait
    // la version d'origine inutilisable. Demarrer l'un suspend l'autre.
    enum class CountdownOwner { Aucun, Programme, Minuteur };
    CountdownOwner m_countdownOwner = CountdownOwner::Aucun;
    void takeCountdownOwnership(CountdownOwner owner);

    void loadSettings();
    void saveSettings();
    void applyProfile(const Profile& profile);
    void refreshPhaseList();
    void refreshDisplay();
    void setBigDisplay(const QString& text, const QColor& color);
    void updateWindowTitle();

    // Marque le programme comme modifie depuis son dernier enregistrement.
    void setProfileModified(bool modified);

    // Propose d'enregistrer si le programme a ete modifie. Renvoie false si
    // l'operateur annule -- la fermeture doit alors etre abandonnee.
    bool confirmSaveIfModified();

    // Archive dans l'historique du programme ce qui vient d'etre deroule.
    // Appelee a la fermeture et au changement de programme, pas a la remise a
    // zero : remettre a zero en plein culte est une correction, pas la fin
    // d'une seance.
    void recordSessionHistory();

    // Enregistre ou efface l'etat de reprise. Appelee aux trois seuls moments
    // qui comptent -- changement de phase, changement d'etat, ajustement de
    // duree -- puisque le temps ecoule se reconstitue ensuite sur l'horloge.
    void saveSessionState();

    // Etat metier
    Clock m_clock{QStringLiteral("Horloge")};
    Timer m_quickTimer{QStringLiteral("Minuteur"), 0};
    PhaseManager m_phases;
    Alarm m_alarm;
    ColorManager m_colors;
    ServiceSchedule m_schedule;
    OutputEngine m_output;
    // Declare apres m_output : MessageCenter recoit son adresse a la
    // construction, l'ordre de declaration determine l'ordre d'initialisation.
    MessageCenter m_messages{&m_output};
    Profile m_profile;
    QString m_profilePath;
    bool m_profileModified = false;
    ProgrammeLibrary m_library{ProgrammeLibrary::defaultDirectory()};

    // Interface
    QListWidget* m_phaseList = nullptr;
    QLabel* m_bigDisplay = nullptr;
    QLabel* m_phaseLabel = nullptr;
    QLabel* m_nextLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_scheduleLabel = nullptr;

    QPushButton* m_btnStartPause = nullptr;
    QPushButton* m_btnPrevious = nullptr;
    QPushButton* m_btnNext = nullptr;
    QPushButton* m_btnAddMinute = nullptr;
    QPushButton* m_btnRemoveMinute = nullptr;
    QPushButton* m_btnReset = nullptr;
    QLineEdit* m_quickDuration = nullptr;
    QPushButton* m_btnQuickStartPause = nullptr;
    QPushButton* m_btnQuickReset = nullptr;
    QAction* m_actOverlay = nullptr;

    MessagePanel* m_messagePanel = nullptr;
    OutputPathsDialog* m_pathsDialog = nullptr;
    HelpDialog* m_helpDialog = nullptr;
    OverlayWindow* m_overlay = nullptr;

    bool m_writeErrorShown = false;
    bool m_geometryClamped = false;
};
