#include "MainWindow.h"
#include "OverlayWindow.h"
#include "SettingsDialog.h"
#include "ProgrammeDialog.h"
#include "MessagePanel.h"
#include "OutputPathsDialog.h"
#include "ProgrammeLibraryDialog.h"
#include "HelpDialog.h"
#include "../utils/CSVExporter.h"
#include "../utils/Autostart.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QGroupBox>
#include <QDockWidget>
#include <QScrollArea>
#include <QFrame>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QGuiApplication>

namespace {

QString defaultOutputDir()
{
    // AppDataLocation contient deja le nom de l'application : y rajouter
    // "TimeOverlay" produisait .../TimeOverlay/TimeOverlay, un dossier qui ne
    // correspondait ni au README ni au script OBS.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::home().filePath(QStringLiteral("TimeOverlay"));
    }
    return QDir(dir).filePath(QStringLiteral("obs"));
}

const char* kSettingsOrg = "TimeOverlay";
const char* kSettingsApp = "TimeOverlay";

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("TimeOverlay - Régie"));

    // Taille souhaitee, affinee apres affichage par showEvent() une fois le
    // cadre de la fenetre connu.
    resize(1340, 780);

    buildUi();
    buildMessageDock();
    buildMenus();
    buildToolbar();

    // L'overlay doit exister avant tout demarrage d'horloge : le premier tick
    // est emis de facon synchrone et le slot y accede.
    m_overlay = createOverlayWindow(nullptr);

    loadSettings();
    wireSignals();

    m_clock.start();
    refreshDisplay();
}

MainWindow::~MainWindow()
{
    delete m_overlay;
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);

    // --- Colonne de gauche : le programme ---
    auto* leftBox = new QGroupBox(tr("Programme"), central);
    // Largeur minimale : sans elle, la rangee de raccourcis du minuteur
    // rapide comprimait cette colonne au point de tronquer les noms de phase.
    leftBox->setMinimumWidth(250);
    auto* leftLayout = new QVBoxLayout(leftBox);
    m_phaseList = new QListWidget(leftBox);
    m_phaseList->setAlternatingRowColors(true);
    leftLayout->addWidget(m_phaseList);
    root->addWidget(leftBox, 1);

    // --- Colonne de droite : l'affichage et les commandes ---
    auto* rightLayout = new QVBoxLayout();

    m_phaseLabel = new QLabel(tr("Aucun programme chargé"), central);
    m_phaseLabel->setAlignment(Qt::AlignCenter);
    m_phaseLabel->setStyleSheet("font-size: 26px; font-weight: bold;");

    m_bigDisplay = new QLabel(QStringLiteral("00:00:00"), central);
    m_bigDisplay->setAlignment(Qt::AlignCenter);
    m_bigDisplay->setStyleSheet("font-size: 96px; font-weight: bold; color: #22c55e;");

    m_nextLabel = new QLabel(QString(), central);
    m_nextLabel->setAlignment(Qt::AlignCenter);
    m_nextLabel->setStyleSheet("font-size: 18px; color: #888888;");

    m_statusLabel = new QLabel(tr("Prêt"), central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 16px; color: #666666;");

    // Decompte avant le debut du culte : masque tant qu'aucune heure n'est
    // programmee, pour ne pas encombrer l'ecran de regie.
    m_scheduleLabel = new QLabel(QString(), central);
    m_scheduleLabel->setAlignment(Qt::AlignCenter);
    m_scheduleLabel->setStyleSheet("font-size: 20px; color: #2563eb; font-weight: bold;");
    m_scheduleLabel->hide();

    rightLayout->addStretch();
    rightLayout->addWidget(m_scheduleLabel);
    rightLayout->addWidget(m_phaseLabel);
    rightLayout->addWidget(m_bigDisplay);
    rightLayout->addWidget(m_nextLabel);
    rightLayout->addWidget(m_statusLabel);
    rightLayout->addStretch();

    // --- Commandes de phase ---
    auto* controls = new QHBoxLayout();
    m_btnPrevious      = new QPushButton(tr("< Précédent"), central);
    m_btnStartPause    = new QPushButton(tr("Démarrer"), central);
    m_btnNext          = new QPushButton(tr("Suivant >"), central);
    m_btnRemoveMinute  = new QPushButton(tr("- 1 min"), central);
    m_btnAddMinute     = new QPushButton(tr("+ 1 min"), central);
    m_btnReset         = new QPushButton(tr("Remise à zéro"), central);

    m_btnStartPause->setMinimumHeight(48);
    m_btnStartPause->setStyleSheet("font-size: 18px; font-weight: bold;");

    controls->addWidget(m_btnPrevious);
    controls->addWidget(m_btnStartPause, 2);
    controls->addWidget(m_btnNext);
    controls->addWidget(m_btnRemoveMinute);
    controls->addWidget(m_btnAddMinute);
    controls->addWidget(m_btnReset);
    rightLayout->addLayout(controls);

    // --- Minuteur libre, sur toute la largeur ---
    rightLayout->addWidget(buildQuickTimerBox(central));

    root->addLayout(rightLayout, 3);
    setCentralWidget(central);
    statusBar()->showMessage(tr("Prêt"));
}

QWidget* MainWindow::buildQuickTimerBox(QWidget* parent)
{
    auto* box = new QGroupBox(tr("Minuteur libre"), parent);
    auto* layout = new QVBoxLayout(box);

    auto* firstRow = new QHBoxLayout();
    m_quickDuration = new QLineEdit(QStringLiteral("5"), box);
    m_quickDuration->setPlaceholderText(tr("5  ou  05:30  ou  01:15:00"));
    m_quickDuration->setToolTip(
        tr("Un nombre seul est compris en minutes. \"05:30\" pour 5 min 30 s, "
           "\"01:15:00\" pour 1 h 15."));
    m_quickDuration->setMaximumWidth(140);

    m_btnQuickStartPause = new QPushButton(tr("Démarrer"), box);
    m_btnQuickStartPause->setMinimumHeight(34);
    m_btnQuickStartPause->setStyleSheet("font-weight: bold;");
    m_btnQuickReset = new QPushButton(tr("Arrêter"), box);

    firstRow->addWidget(new QLabel(tr("Durée :"), box));
    firstRow->addWidget(m_quickDuration);
    firstRow->addWidget(m_btnQuickStartPause, 1);
    firstRow->addWidget(m_btnQuickReset);
    layout->addLayout(firstRow);

    // Raccourcis pour les durees qu'on demande le plus souvent en regie.
    auto* presetRow = new QHBoxLayout();
    presetRow->addWidget(new QLabel(tr("Rapide :"), box));
    // Six raccourcis suffisent : au-dela, la rangee elargit la fenetre plus
    // qu'elle ne rend service, et la saisie libre couvre le reste.
    for (const int minutes : { 1, 2, 5, 10, 15, 30 }) {
        auto* preset = new QPushButton(tr("%1 min").arg(minutes), box);
        preset->setMaximumWidth(58);
        connect(preset, &QPushButton::clicked, this, [this, minutes]() {
            m_quickDuration->setText(QString::number(minutes));
            // Un clic sur un raccourci lance directement : en direct, on
            // n'a pas le temps de cliquer deux fois.
            m_quickTimer.reset();
            onQuickStartPause();
        });
        presetRow->addWidget(preset);
    }
    presetRow->addStretch();
    layout->addLayout(presetRow);

    // Entree valide la saisie, comme dans n'importe quel champ.
    connect(m_quickDuration, &QLineEdit::returnPressed, this, [this]() {
        m_quickTimer.reset();
        onQuickStartPause();
    });

    return box;
}

void MainWindow::buildMessageDock()
{
    // Panneau ancre plutot qu'une fenetre separee : pendant le direct, il faut
    // pouvoir taper un message sans perdre de vue le compte a rebours.
    auto* dock = new QDockWidget(tr("Messages et annonces"), this);
    dock->setObjectName(QStringLiteral("messageDock"));
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    m_messagePanel = new MessagePanel(&m_messages, dock);

    // Le panneau est place dans une zone defilante : son contenu est haut, et
    // sans cela il imposait a toute la fenetre une hauteur minimale superieure
    // a l'ecran d'un portable en 1366x768, rognant la barre d'etat.
    auto* scroll = new QScrollArea(dock);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_messagePanel);
    dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(m_messagePanel, &MessagePanel::settingsChanged, this, &MainWindow::saveSettings);
}

void MainWindow::buildMenus()
{
    auto* menuProgramme = menuBar()->addMenu(tr("&Programme"));
    menuProgramme->addAction(tr("&Nouveau..."), this, &MainWindow::onNewProgramme);

    // La surcharge addAction(texte, raccourci, recepteur, slot) n'existe qu'a
    // partir de Qt 6.3. Ubuntu 22.04 livre Qt 6.2 : on passe par setShortcut
    // pour rester compilable sur les distributions encore en service.
    QAction* actLibrary = menuProgramme->addAction(tr("&Mes programmes..."), this,
                                                   &MainWindow::onOpenLibrary);
    actLibrary->setShortcut(QKeySequence::Open);

    menuProgramme->addAction(tr("Ouvrir un &fichier..."), this, &MainWindow::onLoadProgramme);

    menuProgramme->addSeparator();
    QAction* actSave = menuProgramme->addAction(tr("&Enregistrer"), this, &MainWindow::onSaveProgramme);
    actSave->setShortcut(QKeySequence::Save);

    QAction* actSaveAs = menuProgramme->addAction(tr("Enregistrer une &copie..."), this,
                                                  &MainWindow::onSaveProgrammeAs);
    actSaveAs->setShortcut(QKeySequence::SaveAs);

    menuProgramme->addSeparator();
    menuProgramme->addAction(tr("&Modifier les phases..."), this, &MainWindow::onEditProgramme);
    menuProgramme->addSeparator();
    menuProgramme->addAction(tr("Exporter le &compte rendu..."), this, &MainWindow::onExportSession);
    menuProgramme->addSeparator();

    QAction* actQuit = menuProgramme->addAction(tr("&Quitter"), this, &QWidget::close);
    actQuit->setShortcut(QKeySequence::Quit);

    auto* menuAffichage = menuBar()->addMenu(tr("&Affichage"));
    m_actOverlay = menuAffichage->addAction(tr("Afficher l'&overlay"));
    m_actOverlay->setCheckable(true);
    connect(m_actOverlay, &QAction::triggered, this, &MainWindow::onToggleOverlay);
    menuAffichage->addAction(tr("&Chemins des fichiers pour OBS..."), this,
                             &MainWindow::onShowOutputPaths);
    menuAffichage->addAction(tr("Ouvrir le &dossier de sortie"), this, &MainWindow::onOpenOutputFolder);

    auto* menuOutils = menuBar()->addMenu(tr("&Outils"));
    menuOutils->addAction(tr("&Paramètres..."), this, &MainWindow::onOpenSettings);

    auto* menuAide = menuBar()->addMenu(tr("Aid&e"));
    QAction* actHelp = menuAide->addAction(tr("&Mode d'emploi"), this, &MainWindow::onShowHelp);
    actHelp->setShortcut(QKeySequence::HelpContents);
    menuAide->addAction(tr("À &propos"), this, &MainWindow::onAbout);
}

void MainWindow::buildToolbar()
{
    auto* toolbar = addToolBar(tr("Actions"));
    // saveState() et restoreState() identifient les barres et panneaux par
    // leur objectName. Sans lui, Qt emet un avertissement et la disposition
    // n'est pas restauree au lancement suivant.
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    // Les boutons de commande vivent dans la zone centrale ; la barre d'outils
    // ne sert qu'aux raccourcis globaux. L'ancienne version inserait la barre
    // d'outils elle-meme dans le layout central, ce qui la sortait de sa zone
    // d'ancrage.
    toolbar->addAction(tr("Mes programmes"), this, &MainWindow::onOpenLibrary);
    toolbar->addAction(tr("Modifier"), this, &MainWindow::onEditProgramme);
    toolbar->addSeparator();
    toolbar->addAction(tr("Chemins OBS"), this, &MainWindow::onShowOutputPaths);
    toolbar->addAction(tr("Overlay"), this, &MainWindow::onToggleOverlay);
    toolbar->addAction(tr("Paramètres"), this, &MainWindow::onOpenSettings);
    toolbar->addAction(tr("Aide"), this, &MainWindow::onShowHelp);
}

void MainWindow::wireSignals()
{
    connect(&m_clock, &Clock::tick, this, &MainWindow::onClockTick);

    connect(&m_phases, &PhaseManager::tick, this, &MainWindow::onPhaseTick);
    connect(&m_phases, &PhaseManager::phaseChanged, this, &MainWindow::onPhaseChanged);
    connect(&m_phases, &PhaseManager::phaseFinished, this, &MainWindow::onPhaseFinished);
    connect(&m_phases, &PhaseManager::allPhasesFinished, this, &MainWindow::onAllPhasesFinished);

    connect(&m_schedule, &ServiceSchedule::tick, this, &MainWindow::onScheduleTick);
    connect(&m_schedule, &ServiceSchedule::startTimeReached, this, &MainWindow::onStartTimeReached);

    connect(&m_output, &OutputEngine::writeFailed, this, &MainWindow::onWriteFailed);
    connect(&m_alarm, &Alarm::soundError, this, [this](const QString& message) {
        statusBar()->showMessage(tr("Son d'alarme indisponible : %1").arg(message), 8000);
    });

    connect(m_btnStartPause, &QPushButton::clicked, this, &MainWindow::onStartPause);
    connect(m_btnNext, &QPushButton::clicked, this, &MainWindow::onNextPhase);
    connect(m_btnPrevious, &QPushButton::clicked, this, &MainWindow::onPreviousPhase);
    connect(m_btnAddMinute, &QPushButton::clicked, this, &MainWindow::onAddMinute);
    connect(m_btnRemoveMinute, &QPushButton::clicked, this, &MainWindow::onRemoveMinute);
    connect(m_btnReset, &QPushButton::clicked, this, &MainWindow::onResetProgramme);
    connect(m_btnQuickStartPause, &QPushButton::clicked, this, &MainWindow::onQuickStartPause);
    connect(m_btnQuickReset, &QPushButton::clicked, this, &MainWindow::onQuickReset);
    connect(&m_quickTimer, &Timer::tick, this, &MainWindow::onQuickTick);
    connect(&m_quickTimer, &Timer::finished, this, &MainWindow::onQuickFinished);

    connect(m_phaseList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        m_phases.goToPhase(m_phaseList->row(item));
    });
}

// ---------------------------------------------------------------- reglages

void MainWindow::loadSettings()
{
    QSettings settings(kSettingsOrg, kSettingsApp);

    const QString dir = settings.value("output/dir", defaultOutputDir()).toString();
    if (!m_output.setBaseDir(dir)) {
        // Dossier configure inutilisable : on retombe sur celui par defaut
        // plutot que de laisser l'application ecrire dans le vide.
        m_output.setBaseDir(defaultOutputDir());
    }

    m_clock.setTimeFormat(settings.value("format/time", Clock::defaultTimeFormat()).toString());
    m_clock.setDateFormat(settings.value("format/date", Clock::defaultDateFormat()).toString());

    m_colors.setWarningThreshold(settings.value("colors/warning", 300).toInt());
    m_colors.setCriticalThreshold(settings.value("colors/critical", 60).toInt());
    m_colors.setColors(QColor(settings.value("colors/normal", "#22c55e").toString()),
                       QColor(settings.value("colors/warningColor", "#f59e0b").toString()),
                       QColor(settings.value("colors/criticalColor", "#ef4444").toString()),
                       QColor(settings.value("colors/overtimeColor", "#dc2626").toString()));

    m_alarm.setEnabled(settings.value("alarm/enabled", true).toBool());
    m_alarm.setSoundFile(settings.value("alarm/sound", QString()).toString());
    m_phases.setAutoAdvance(settings.value("programme/autoAdvance", false).toBool());

    // Messages et annonces : ce sont les memes d'un culte a l'autre, il serait
    // absurde de les retaper chaque dimanche.
    if (m_messagePanel) {
        m_messagePanel->setTemplates(settings.value("messages/templates").toStringList());
        m_messagePanel->setAnnouncements(settings.value("messages/announcements").toStringList());
    }
    m_messages.setRotationSeconds(settings.value("messages/rotationSeconds", 10).toInt());

    TextFormat messageFormat;
    messageFormat.maxCharsPerLine = settings.value("messages/messageWidth", 0).toInt();
    messageFormat.maxLines = settings.value("messages/messageLines", 0).toInt();
    messageFormat.overflow = static_cast<TextFormat::Overflow>(
        settings.value("messages/messageOverflow", 0).toInt());
    messageFormat.uppercase = settings.value("messages/messageUppercase", false).toBool();
    m_messages.setMessageFormat(messageFormat);

    TextFormat announcementFormat;
    announcementFormat.maxCharsPerLine = settings.value("messages/annonceWidth", 0).toInt();
    announcementFormat.maxLines = settings.value("messages/annonceLines", 0).toInt();
    announcementFormat.overflow = static_cast<TextFormat::Overflow>(
        settings.value("messages/annonceOverflow", 0).toInt());
    announcementFormat.uppercase = settings.value("messages/annonceUppercase", false).toBool();
    m_messages.setAnnouncementFormat(announcementFormat);

    // La diffusion des annonces n'est jamais reactivee toute seule au
    // demarrage : elle doit etre un geste volontaire de l'operateur.
    m_messages.setRotationEnabled(false);

    if (m_messagePanel) {
        m_messagePanel->applyStoredState(messageFormat, announcementFormat,
                                         m_messages.rotationSeconds());
    }

    restoreGeometry(settings.value("window/geometry").toByteArray());
    restoreState(settings.value("window/state").toByteArray());

    statusBar()->showMessage(tr("Dossier de sortie : %1").arg(m_output.baseDir()));

    // Premier lancement : l'application s'inscrit au demarrage de la session.
    // Elle affiche l'heure et le decompte avant le culte -- elle ne sert a
    // rien si personne ne pense a l'ouvrir. La decision n'est prise qu'une
    // seule fois : si l'operateur decoche la case ensuite, on ne la recoche
    // pas au lancement suivant.
    if (Autostart::isSupported() && !settings.contains("startup/autostartConfigured")) {
        settings.setValue("startup/autostartConfigured", true);
        if (Autostart::setEnabled(true)) {
            statusBar()->showMessage(
                tr("TimeOverlay s'ouvrira désormais au démarrage de l'ordinateur. "
                   "Réglage modifiable dans Outils ▸ Paramètres."), 15000);
        }
    }
}

void MainWindow::saveSettings()
{
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue("output/dir", m_output.baseDir());
    settings.setValue("format/time", m_clock.timeFormat());
    settings.setValue("format/date", m_clock.dateFormat());
    settings.setValue("colors/warning", m_colors.warningThreshold());
    settings.setValue("colors/critical", m_colors.criticalThreshold());
    settings.setValue("colors/normal", m_colors.normalColor().name());
    settings.setValue("colors/warningColor", m_colors.warningColor().name());
    settings.setValue("colors/criticalColor", m_colors.criticalColor().name());
    settings.setValue("colors/overtimeColor", m_colors.overtimeColor().name());
    settings.setValue("alarm/enabled", m_alarm.isEnabled());
    settings.setValue("alarm/sound", m_alarm.soundFile());
    settings.setValue("programme/autoAdvance", m_phases.autoAdvance());

    if (m_messagePanel) {
        settings.setValue("messages/templates", m_messagePanel->templates());
        settings.setValue("messages/announcements", m_messagePanel->announcements());
    }
    settings.setValue("messages/rotationSeconds", m_messages.rotationSeconds());

    const TextFormat messageFormat = m_messages.messageFormat();
    settings.setValue("messages/messageWidth", messageFormat.maxCharsPerLine);
    settings.setValue("messages/messageLines", messageFormat.maxLines);
    settings.setValue("messages/messageOverflow", int(messageFormat.overflow));
    settings.setValue("messages/messageUppercase", messageFormat.uppercase);

    const TextFormat announcementFormat = m_messages.announcementFormat();
    settings.setValue("messages/annonceWidth", announcementFormat.maxCharsPerLine);
    settings.setValue("messages/annonceLines", announcementFormat.maxLines);
    settings.setValue("messages/annonceOverflow", int(announcementFormat.overflow));
    settings.setValue("messages/annonceUppercase", announcementFormat.uppercase);

    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_geometryClamped) return;
    m_geometryClamped = true;

    // resize() dimensionne la zone client, mais c'est le CADRE -- barre de
    // titre et bordures comprises -- qui doit tenir dans l'ecran. Sur un
    // portable en 1366x768 ce cadre ajoutait 39 px et la barre d'etat
    // disparaissait sous la barre des taches. On ne peut le mesurer qu'une
    // fois la fenetre affichee.
    const QScreen* ecran = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!ecran) return;

    const QRect zone = ecran->availableGeometry();
    const QRect cadre = frameGeometry();
    if (cadre.width() <= zone.width() && cadre.height() <= zone.height()) return;

    const int bordureH = cadre.width() - width();
    const int bordureV = cadre.height() - height();
    resize(qMin(width(), zone.width() - bordureH),
           qMin(height(), zone.height() - bordureV));
    move(zone.topLeft());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // La question de l'enregistrement vient en premier : elle peut annuler la
    // fermeture, et tout ce qui suit serait alors fait pour rien.
    if (!confirmSaveIfModified()) {
        event->ignore();
        return;
    }

    recordSessionHistory();

    // Derniere ecriture avant de rendre la main : les fichiers doivent
    // refleter l'etat final, pas celui du dernier cycle.
    m_output.flushNow();
    saveSettings();
    QMainWindow::closeEvent(event);
}

// ------------------------------------------- enregistrement et historique

void MainWindow::updateWindowTitle()
{
    if (m_profile.phases.isEmpty()) {
        setWindowTitle(tr("TimeOverlay - Régie"));
        return;
    }
    // L'asterisque est la convention partout ailleurs pour « modifie, non
    // enregistre ». Autant s'en servir plutot qu'inventer un signe a
    // expliquer.
    setWindowTitle(tr("%1%2 - TimeOverlay")
                       .arg(m_profile.name, m_profileModified ? QStringLiteral(" *") : QString()));
}

void MainWindow::setProfileModified(bool modified)
{
    m_profileModified = modified;
    updateWindowTitle();
}

bool MainWindow::confirmSaveIfModified()
{
    if (!m_profileModified || m_profile.phases.isEmpty()) return true;

    // Boutons libelles a la main plutot que QMessageBox::Save et compagnie :
    // ces derniers tirent leur texte du catalogue de traduction de Qt, qui
    // n'est pas toujours trouve selon le mode d'installation. Sur la seule
    // boite qui decide si le travail est perdu, « Discard » en anglais au
    // milieu d'une phrase francaise est un risque de mauvais clic.
    QMessageBox box(QMessageBox::Question, tr("Enregistrer les modifications ?"),
                    tr("Le programme « %1 » a été modifié.\n\n"
                       "Voulez-vous enregistrer les changements ?").arg(m_profile.name),
                    QMessageBox::NoButton, this);

    QPushButton* save = box.addButton(tr("Enregistrer"), QMessageBox::AcceptRole);
    QPushButton* discard = box.addButton(tr("Ne pas enregistrer"), QMessageBox::DestructiveRole);
    box.addButton(tr("Annuler"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.exec();

    if (box.clickedButton() == discard) {
        setProfileModified(false);
        return true;
    }
    if (box.clickedButton() != save) return false;   // Annuler, ou fenetre fermee

    onSaveProgramme();
    // Si l'enregistrement a echoue, le drapeau est encore leve : on ne ferme
    // pas en silence sur une erreur que l'operateur vient de voir.
    return !m_profileModified;
}

void MainWindow::recordSessionHistory()
{
    // La phase en cours n'a pas encore ete archivee : elle n'a pas ete
    // quittee. C'est presque toujours la derniere du culte.
    m_phases.endSession();

    if (m_phases.sessionLog().isEmpty() || m_profile.name.isEmpty()) return;

    QString error;
    if (!m_library.appendSession(m_profile.name, m_phases.sessionLog(), &error)) {
        statusBar()->showMessage(tr("Historique non enregistré : %1").arg(error), 8000);
    }
    m_phases.clearSessionLog();
}

// ------------------------------------------------------------- programme

void MainWindow::applyProfile(const Profile& profile)
{
    // Ce qui vient d'etre deroule appartient au programme qu'on quitte : il
    // faut l'archiver avant que loadProfile() n'efface le compte rendu.
    recordSessionHistory();

    m_profile = profile;
    m_phases.loadProfile(profile);
    refreshPhaseList();
    updateWindowTitle();

    // Le decompte avant le debut repart a chaque chargement de programme.
    m_schedule.setAutoStartEnabled(profile.autoStart);
    m_schedule.setStartTime(profile.startTimeAsTime());

    m_phaseLabel->setText(profile.name.isEmpty() ? tr("Programme") : profile.name);
    m_nextLabel->setText(profile.phases.isEmpty()
                             ? QString()
                             : tr("Première phase : %1").arg(profile.phases.first().name));
    refreshDisplay();
}

void MainWindow::refreshPhaseList()
{
    m_phaseList->clear();
    for (int i = 0; i < m_profile.phases.size(); ++i) {
        const Phase& p = m_profile.phases[i];
        m_phaseList->addItem(tr("%1. %2  (%3)")
                                 .arg(i + 1)
                                 .arg(p.name, Timer::format(p.durationSeconds)));
    }
    if (m_phases.currentPhaseIndex() >= 0) {
        m_phaseList->setCurrentRow(m_phases.currentPhaseIndex());
    }
}

void MainWindow::onNewProgramme()
{
    if (!confirmSaveIfModified()) return;

    Profile fresh;
    fresh.name = tr("Nouveau programme");
    ProgrammeDialog dialog(fresh, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const Profile created = dialog.profile();

    // La bibliotheque est indexee par nom : un nom deja pris remplacerait le
    // programme existant. On le dit avant, pas apres.
    const QString target = m_library.pathFor(created.name);
    if (!target.isEmpty() && QFileInfo::exists(target)) {
        const auto answer = QMessageBox::question(
            this, tr("Nom déjà utilisé"),
            tr("Un programme nommé « %1 » existe déjà dans votre bibliothèque.\n\n"
               "Le remplacer ?").arg(created.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    // Enregistrement immediat : un programme qu'on vient de saisir ne doit pas
    // pouvoir disparaitre parce qu'on a oublie de l'enregistrer.
    QString savedPath;
    QString error;
    if (!m_library.save(created, &savedPath, &error)) {
        QMessageBox::warning(this, tr("Enregistrement impossible"),
                             tr("Le programme n'a pas pu être rangé dans la bibliothèque :\n%1\n\n"
                                "Il reste chargé, mais il faudra l'enregistrer à la main.")
                                 .arg(error));
        m_profilePath.clear();
        applyProfile(created);
        setProfileModified(true);
        return;
    }

    m_profilePath = savedPath;
    applyProfile(created);
    setProfileModified(false);
    statusBar()->showMessage(tr("« %1 » enregistré dans vos programmes").arg(created.name), 6000);
}

void MainWindow::onEditProgramme()
{
    if (m_profile.phases.isEmpty()) {
        QMessageBox::information(
            this, tr("Aucun programme"),
            tr("Créez un programme avec « Programme ▸ Nouveau », ou ouvrez-en un depuis "
               "« Mes programmes »."));
        return;
    }

    ProgrammeDialog dialog(m_profile, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const Profile edited = dialog.profile();

    // Comparaison sur la representation JSON : c'est deja la forme
    // canonique du programme, celle qui part sur le disque. Elle evite
    // d'ecrire des operateurs de comparaison qui n'auraient pas d'autre usage.
    if (edited.toJson() == m_profile.toJson()) return;

    applyProfile(edited);
    setProfileModified(true);
    statusBar()->showMessage(
        tr("Programme modifié. Il sera proposé à l'enregistrement en quittant "
           "(ou tout de suite avec Ctrl+S)."), 8000);
}

void MainWindow::onOpenLibrary()
{
    ProgrammeLibraryDialog dialog(&m_library, this);
    if (dialog.exec() != QDialog::Accepted) return;

    // La question de l'enregistrement vient apres le choix, pas avant :
    // consulter la bibliotheque puis renoncer ne doit rien declencher.
    if (!confirmSaveIfModified()) return;
    openProgrammeFile(dialog.selectedPath());
}

bool MainWindow::openProgrammeFile(const QString& path)
{
    if (path.isEmpty()) return false;

    QString error;
    const Profile profile = Profile::fromFile(path, &error);
    if (!profile.isValid()) {
        QMessageBox::warning(this, tr("Chargement impossible"), error);
        return false;
    }

    m_profilePath = path;
    applyProfile(profile);
    setProfileModified(false);
    statusBar()->showMessage(tr("%1 phases chargées").arg(profile.phases.size()), 5000);
    return true;
}

void MainWindow::onLoadProgramme()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Ouvrir un programme"), QDir::homePath(),
        tr("Programmes (*.timerproject *.json);;Tous les fichiers (*)"));
    if (path.isEmpty()) return;

    if (!confirmSaveIfModified()) return;
    openProgrammeFile(path);
}

void MainWindow::onSaveProgramme()
{
    if (m_profile.phases.isEmpty()) {
        QMessageBox::information(this, tr("Rien à enregistrer"),
                                 tr("Le programme ne contient aucune phase."));
        return;
    }

    QString error;

    // Programme ouvert depuis un fichier de l'utilisateur : on reecrit ce
    // fichier-la. Sinon il rejoint la bibliotheque, sous son nom.
    if (!m_profilePath.isEmpty() && !m_library.contains(m_profilePath)) {
        if (!m_profile.saveToFile(m_profilePath, &error)) {
            QMessageBox::warning(this, tr("Enregistrement impossible"), error);
            return;
        }
    } else {
        const QString previous = m_profilePath;
        const QString target = m_library.pathFor(m_profile.name);

        // Renommer un programme puis l'enregistrer en laissait deux dans la
        // bibliotheque : l'ancien avec tout l'historique, le nouveau vide. On
        // ne devine pas l'intention -- garder les deux est parfois voulu.
        bool dropPrevious = false;
        if (m_library.contains(previous) && !target.isEmpty()
            && QDir::cleanPath(previous) != QDir::cleanPath(target)) {

            const QString previousName = Profile::fromFile(previous).name;

            QMessageBox box(QMessageBox::Question, tr("Le programme a changé de nom"),
                            tr("Ce programme s'appelait « %1 » et s'appelle maintenant « %2 ».\n\n"
                               "Le renommer, ou garder les deux ?")
                                .arg(previousName, m_profile.name),
                            QMessageBox::NoButton, this);
            QPushButton* rename = box.addButton(tr("Renommer"), QMessageBox::AcceptRole);
            QPushButton* keep = box.addButton(tr("Garder les deux"), QMessageBox::AcceptRole);
            box.addButton(tr("Annuler"), QMessageBox::RejectRole);
            box.setDefaultButton(rename);
            box.exec();

            if (box.clickedButton() == rename)      dropPrevious = true;
            else if (box.clickedButton() != keep)   return;
        }

        QString savedPath;
        if (!m_library.save(m_profile, &savedPath, &error)) {
            QMessageBox::warning(this, tr("Enregistrement impossible"), error);
            return;
        }
        m_profilePath = savedPath;

        // Le nouveau fichier existe : l'ancien peut partir, et son historique
        // avec lui. Un echec ici n'annule pas l'enregistrement, qui a reussi.
        if (dropPrevious && !m_library.dropPreviousName(previous, m_profile.name, &error)) {
            statusBar()->showMessage(
                tr("Programme enregistré, mais l'ancien nom n'a pas pu être retiré : %1")
                    .arg(error), 10000);
            setProfileModified(false);
            return;
        }
    }

    setProfileModified(false);
    statusBar()->showMessage(tr("« %1 » enregistré").arg(m_profile.name), 5000);
}

void MainWindow::onSaveProgrammeAs()
{
    if (m_profile.phases.isEmpty()) {
        QMessageBox::information(this, tr("Rien à enregistrer"),
                                 tr("Le programme ne contient aucune phase."));
        return;
    }

    // Une copie hors bibliotheque : pour emporter le programme sur une clé, ou
    // le donner a quelqu'un. Le programme charge reste rattache a son propre
    // fichier, sinon un export deplacerait sans le dire ce sur quoi on
    // travaille.
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Enregistrer une copie"),
        QDir::home().filePath(m_profile.name + QStringLiteral(".timerproject")),
        tr("Programmes (*.timerproject)"));
    if (path.isEmpty()) return;

    QString error;
    if (!m_profile.saveToFile(path, &error)) {
        QMessageBox::warning(this, tr("Enregistrement impossible"), error);
        return;
    }
    statusBar()->showMessage(tr("Copie enregistrée : %1").arg(path), 5000);
}

void MainWindow::onExportSession()
{
    if (m_phases.sessionLog().isEmpty()) {
        QMessageBox::information(this, tr("Aucune donnée"),
                                 tr("Aucune phase n'a encore été déroulée."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter le compte rendu"),
        QDir::home().filePath(tr("compte-rendu.csv")),
        tr("Fichiers CSV (*.csv)"));
    if (path.isEmpty()) return;

    QString error;
    if (!CSVExporter::exportSession(path, m_phases.sessionLog(), &error)) {
        QMessageBox::warning(this, tr("Export impossible"), error);
        return;
    }
    statusBar()->showMessage(tr("Compte rendu exporté"), 5000);
}

// -------------------------------------------------------------- commandes

void MainWindow::onStartPause()
{
    const Timer::State state = m_phases.currentState();
    if (state == Timer::State::RUNNING || state == Timer::State::OVERTIME) {
        m_phases.pause();
    } else {
        if (m_profile.phases.isEmpty()) {
            QMessageBox::information(this, tr("Aucun programme"),
                                     tr("Chargez ou créez un programme avant de démarrer."));
            return;
        }
        m_phases.start();
        // Demarrage manuel : le decompte avant le debut n'a plus lieu d'etre.
        m_schedule.markStarted();
    }
    refreshDisplay();
}

void MainWindow::onNextPhase()      { m_phases.nextPhase(); }
void MainWindow::onPreviousPhase()  { m_phases.previousPhase(); }
void MainWindow::onAddMinute()      { m_phases.addSecondsToCurrentPhase(60); }
void MainWindow::onRemoveMinute()   { m_phases.addSecondsToCurrentPhase(-60); }

void MainWindow::onResetProgramme()
{
    m_phases.reset();
    // Remise a zero complete : le decompte avant le debut reprend si l'heure
    // programmee est encore devant nous.
    m_schedule.reset();
    m_output.clear(OutputEngine::Countdown);
    m_output.clear(OutputEngine::Countup);
    m_output.clear(OutputEngine::Depassement);
    m_output.clear(OutputEngine::Phase);
    m_output.clear(OutputEngine::PhaseSuivante);
    m_output.set(OutputEngine::Statut, PhaseManager::stateLabel(Timer::State::STOPPED));
    refreshDisplay();
}

// ------------------------------------------------------- minuteur rapide

void MainWindow::takeCountdownOwnership(CountdownOwner owner)
{
    if (m_countdownOwner == owner) return;

    // Le proprietaire change AVANT les effets de bord : pause() et reset()
    // emettent un tick, et le gestionnaire de ce tick teste justement le
    // proprietaire pour savoir s'il a le droit d'ecrire. Dans l'autre ordre,
    // le module qu'on vient de suspendre ecrirait une derniere fois.
    const CountdownOwner precedent = m_countdownOwner;
    m_countdownOwner = owner;

    if (owner == CountdownOwner::Minuteur && precedent == CountdownOwner::Programme) {
        m_phases.pause();
        statusBar()->showMessage(
            tr("Programme mis en pause : le minuteur rapide prend la main sur l'affichage"), 8000);
    } else if (owner == CountdownOwner::Programme && precedent == CountdownOwner::Minuteur) {
        m_quickTimer.reset();
        m_btnQuickStartPause->setText(tr("Démarrer"));
        statusBar()->showMessage(
            tr("Minuteur rapide arrêté : le programme reprend la main"), 8000);
    }
}

void MainWindow::onQuickStartPause()
{
    const Timer::State state = m_quickTimer.state();

    if (state == Timer::State::RUNNING || state == Timer::State::OVERTIME) {
        m_quickTimer.pause();
        m_btnQuickStartPause->setText(tr("Reprendre"));
        return;
    }

    // Depuis l'arret on relit la duree saisie ; depuis une pause on reprend
    // sans y toucher, sinon on perdrait le temps deja ecoule.
    if (state == Timer::State::STOPPED || state == Timer::State::FINISHED) {
        int seconds = 0;
        if (!Timer::parseDuration(m_quickDuration->text(), &seconds) || seconds <= 0) {
            QMessageBox::information(
                this, tr("Durée invalide"),
                tr("Indiquez une durée : \"5\" pour 5 minutes, \"05:30\" pour 5 min 30 s, "
                   "ou \"01:15:00\" pour 1 h 15."));
            m_quickDuration->setFocus();
            m_quickDuration->selectAll();
            return;
        }
        m_quickTimer.reset();
        m_quickTimer.setDuration(seconds);
    }

    takeCountdownOwnership(CountdownOwner::Minuteur);
    m_quickTimer.start();
    m_btnQuickStartPause->setText(tr("Pause"));
}

void MainWindow::onQuickReset()
{
    m_quickTimer.reset();
    m_btnQuickStartPause->setText(tr("Démarrer"));

    if (m_countdownOwner != CountdownOwner::Minuteur) return;

    m_countdownOwner = CountdownOwner::Aucun;
    m_output.clear(OutputEngine::Countdown);
    m_output.clear(OutputEngine::Countup);
    m_output.clear(OutputEngine::Depassement);
    m_output.clear(OutputEngine::Phase);
    m_output.set(OutputEngine::Statut, PhaseManager::stateLabel(Timer::State::STOPPED));

    setBigDisplay(QStringLiteral("00:00:00"), m_colors.normalColor());
    m_phaseLabel->setText(m_profile.phases.isEmpty() ? tr("Aucun programme chargé")
                                                     : m_profile.name);
    m_nextLabel->clear();
    refreshDisplay();
}

void MainWindow::onQuickTick(QString countdown, QString overtime, Timer::State state)
{
    if (m_countdownOwner != CountdownOwner::Minuteur) return;

    const bool inOvertime = (state == Timer::State::OVERTIME);

    m_output.set(OutputEngine::Countdown, countdown);
    m_output.set(OutputEngine::Countup, m_quickTimer.countup());
    m_output.set(OutputEngine::Depassement, overtime);
    m_output.set(OutputEngine::Statut, PhaseManager::stateLabel(state));
    m_output.set(OutputEngine::Phase, tr("Minuteur"));
    m_output.clear(OutputEngine::PhaseSuivante);

    const QColor color = m_colors.colorFor(m_quickTimer.remainingSeconds(), inOvertime);
    setBigDisplay(inOvertime && !overtime.isEmpty() ? overtime : countdown, color);

    m_phaseLabel->setText(tr("Minuteur rapide"));
    m_nextLabel->clear();
    m_statusLabel->setText(tr("Minuteur de %1 — %2")
                               .arg(Timer::format(m_quickTimer.durationSeconds()),
                                    PhaseManager::stateLabel(state)));

    if (m_overlay) {
        m_overlay->setTimerText(inOvertime && !overtime.isEmpty() ? overtime : countdown);
        m_overlay->setColor(color);
    }

    m_btnQuickStartPause->setText(
        (state == Timer::State::RUNNING || state == Timer::State::OVERTIME)
            ? tr("Pause") : tr("Reprendre"));
}

void MainWindow::onQuickFinished()
{
    if (m_countdownOwner != CountdownOwner::Minuteur) return;
    m_alarm.trigger();
}

void MainWindow::onShowOutputPaths()
{
    // Fenetre non modale : on la garde ouverte a cote pendant qu'on configure
    // OBS, en copiant les chemins un par un. Une fenetre modale obligerait a
    // la fermer et la rouvrir entre chaque source Texte.
    //
    // Recreee a chaque ouverture pour que les chemins refletent le dossier de
    // sortie courant, qui peut avoir change dans les parametres.
    if (m_pathsDialog) {
        m_pathsDialog->close();
        m_pathsDialog = nullptr;
    }

    m_pathsDialog = new OutputPathsDialog(m_output.baseDir(), this);
    m_pathsDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_pathsDialog, &QObject::destroyed, this, [this]() { m_pathsDialog = nullptr; });
    m_pathsDialog->show();
    m_pathsDialog->raise();
    m_pathsDialog->activateWindow();
}

void MainWindow::onToggleOverlay()
{
    if (!m_overlay) return;

    const bool show = !m_overlay->isVisible();
    m_overlay->setVisible(show);
    if (m_actOverlay) m_actOverlay->setChecked(show);
}

void MainWindow::onOpenOutputFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_output.baseDir()));
}

void MainWindow::onShowHelp()
{
    // Non modale, comme la fenetre des chemins : on suit le mode d'emploi en
    // manipulant l'application a cote. Une seule instance a la fois, ramenee
    // au premier plan si elle est deja ouverte.
    if (m_helpDialog) {
        m_helpDialog->raise();
        m_helpDialog->activateWindow();
        return;
    }

    m_helpDialog = new HelpDialog(m_output.baseDir(), this);
    m_helpDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_helpDialog, &HelpDialog::actionRequested, this, &MainWindow::onHelpAction);
    connect(m_helpDialog, &QObject::destroyed, this, [this]() { m_helpDialog = nullptr; });
    m_helpDialog->show();
}

void MainWindow::onHelpAction(const QString& id)
{
    if (id == QLatin1String("programmes"))      onOpenLibrary();
    else if (id == QLatin1String("nouveau"))    onNewProgramme();
    else if (id == QLatin1String("modifier"))   onEditProgramme();
    else if (id == QLatin1String("chemins"))    onShowOutputPaths();
    else if (id == QLatin1String("dossier"))    onOpenOutputFolder();
    else if (id == QLatin1String("parametres")) onOpenSettings();
    else if (id == QLatin1String("overlay"))    onToggleOverlay();
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, tr("À propos de TimeOverlay"),
        tr("<h3>TimeOverlay %1</h3>"
           "<p>Chronométrage du culte pour OBS : l'heure, un compte à rebours et le "
           "temps de dépassement, écrits dans des fichiers texte que les sources "
           "Texte d'OBS affichent.</p>"
           "<p>Dossier de sortie :<br><span style='color:#555555'>%2</span></p>"
           "<p>Vos programmes :<br><span style='color:#555555'>%3</span></p>")
            .arg(QCoreApplication::applicationVersion(),
                 m_output.baseDir(),
                 m_library.directory()));
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dialog(this);
    dialog.setOutputDir(m_output.baseDir());
    dialog.setTimeFormat(m_clock.timeFormat());
    dialog.setDateFormat(m_clock.dateFormat());
    dialog.setWarningThreshold(m_colors.warningThreshold());
    dialog.setCriticalThreshold(m_colors.criticalThreshold());
    dialog.setColors(m_colors.normalColor(), m_colors.warningColor(),
                     m_colors.criticalColor(), m_colors.overtimeColor());
    dialog.setAlarmEnabled(m_alarm.isEnabled());
    dialog.setAlarmSound(m_alarm.soundFile());
    dialog.setAutoAdvance(m_phases.autoAdvance());
    // Lu depuis le systeme et non depuis QSettings : c'est le systeme qui fait
    // foi, et l'utilisateur a pu retirer l'entree de son cote.
    dialog.setAutostart(Autostart::isEnabled());

    if (dialog.exec() != QDialog::Accepted) return;

    if (Autostart::isSupported() && dialog.autostart() != Autostart::isEnabled()) {
        QString error;
        if (!Autostart::setEnabled(dialog.autostart(), &error)) {
            QMessageBox::warning(
                this, tr("Lancement au démarrage"),
                tr("Le réglage n'a pas pu être appliqué :\n%1").arg(error));
        }
    }

    if (dialog.outputDir() != m_output.baseDir()) {
        if (!m_output.setBaseDir(dialog.outputDir())) {
            QMessageBox::warning(this, tr("Dossier inutilisable"),
                                 tr("Impossible d'écrire dans %1. L'ancien dossier est conservé.")
                                     .arg(dialog.outputDir()));
        }
    }

    m_clock.setTimeFormat(dialog.timeFormat());
    m_clock.setDateFormat(dialog.dateFormat());
    m_colors.setWarningThreshold(dialog.warningThreshold());
    m_colors.setCriticalThreshold(dialog.criticalThreshold());
    m_colors.setColors(dialog.normalColor(), dialog.warningColor(),
                       dialog.criticalColor(), dialog.overtimeColor());
    m_alarm.setEnabled(dialog.alarmEnabled());
    m_alarm.setSoundFile(dialog.alarmSound());
    m_phases.setAutoAdvance(dialog.autoAdvance());

    saveSettings();
    statusBar()->showMessage(tr("Dossier de sortie : %1").arg(m_output.baseDir()));
    refreshDisplay();
}

// ----------------------------------------------------------------- ticks

void MainWindow::onClockTick(QString time, QString date)
{
    // Ces deux cles ne touchent que leurs propres fichiers : l'horloge
    // n'efface plus le countdown, contrairement a l'ancienne version.
    m_output.set(OutputEngine::Heure, time);
    m_output.set(OutputEngine::Date, date);

    if (m_overlay) m_overlay->setClockText(time);
}

void MainWindow::onPhaseTick(QString countdown, QString overtime, Timer::State state)
{
    // Le minuteur rapide a la main : le programme ne doit rien ecrire, sinon
    // les deux s'ecraseraient dans countdown.txt.
    if (m_countdownOwner == CountdownOwner::Minuteur) return;

    Timer* timer = m_phases.timer();
    const bool inOvertime = (state == Timer::State::OVERTIME);
    const int remaining = timer ? timer->remainingSeconds() : 0;
    const bool phaseActive = m_phases.currentPhaseIndex() >= 0;

    m_output.set(OutputEngine::Countdown, countdown);
    m_output.set(OutputEngine::Countup, timer ? timer->countup() : QString());
    m_output.set(OutputEngine::Depassement, overtime);
    m_output.set(OutputEngine::Statut, PhaseManager::stateLabel(state));

    // Tant qu'aucune phase n'est lancee, le temps restant vaut zero et la
    // regle de couleur donnerait du rouge : un afficheur ecarlate avant meme
    // le debut du culte ressemble a une alarme. On reste neutre.
    const QColor color = phaseActive ? m_colors.colorFor(remaining, inOvertime)
                                     : m_colors.normalColor();
    setBigDisplay(inOvertime && !overtime.isEmpty() ? overtime : countdown, color);

    if (phaseActive) {
        m_statusLabel->setText(tr("Phase %1/%2 - %3")
                                   .arg(m_phases.currentPhaseIndex() + 1)
                                   .arg(m_phases.totalPhases())
                                   .arg(PhaseManager::stateLabel(state)));
    } else {
        m_statusLabel->setText(tr("%1 phases - programme non démarré")
                                   .arg(m_phases.totalPhases()));
    }

    if (m_overlay) {
        m_overlay->setTimerText(inOvertime && !overtime.isEmpty() ? overtime : countdown);
        m_overlay->setColor(color);
    }

    m_btnStartPause->setText(
        (state == Timer::State::RUNNING || state == Timer::State::OVERTIME)
            ? tr("Pause") : tr("Démarrer"));
}

void MainWindow::onPhaseChanged(int index, QString name, int durationSeconds)
{
    Q_UNUSED(durationSeconds)

    // Une phase demarre : le programme reprend la main sur l'affichage, ce
    // qui arrete le minuteur rapide s'il tournait.
    takeCountdownOwnership(CountdownOwner::Programme);

    m_output.set(OutputEngine::Phase, name);
    m_output.set(OutputEngine::PhaseSuivante, m_phases.nextPhaseName());
    // Le message affiche appartient a l'operateur : un changement de phase ne
    // doit pas l'effacer sous ses pieds.

    m_phaseLabel->setText(name);
    m_nextLabel->setText(m_phases.nextPhaseName().isEmpty()
                             ? tr("Dernière phase")
                             : tr("Ensuite : %1").arg(m_phases.nextPhaseName()));
    m_phaseList->setCurrentRow(index);

    CSVExporter::appendLog(QDir(m_output.baseDir()).filePath(QStringLiteral("journal")),
                           QStringLiteral("programme"), QStringLiteral("DEBUT_PHASE"), name);
}

void MainWindow::onPhaseFinished(int index, QString name)
{
    Q_UNUSED(index)

    m_alarm.trigger();

    CSVExporter::appendLog(QDir(m_output.baseDir()).filePath(QStringLiteral("journal")),
                           QStringLiteral("programme"), QStringLiteral("FIN_PHASE"), name);
}

void MainWindow::onAllPhasesFinished()
{
    m_statusLabel->setText(tr("Programme terminé"));
    m_output.set(OutputEngine::Statut, QStringLiteral("TERMINE"));
    m_output.clear(OutputEngine::PhaseSuivante);
    statusBar()->showMessage(tr("Toutes les phases sont terminées"), 10000);
}

void MainWindow::onScheduleTick(QString remaining)
{
    m_output.set(OutputEngine::AvantDebut, remaining);

    if (remaining.isEmpty()) {
        m_scheduleLabel->hide();
        return;
    }
    m_scheduleLabel->setText(tr("Le culte commence dans %1").arg(remaining));
    m_scheduleLabel->show();
}

void MainWindow::onStartTimeReached()
{
    if (m_profile.phases.isEmpty()) return;
    // Ne demarre que la premiere phase. Les suivantes restent manuelles :
    // une phase deborde presque toujours, et rien ne doit etre coupe.
    if (m_phases.currentPhaseIndex() >= 0) return;

    m_phases.start();
    refreshDisplay();
    statusBar()->showMessage(tr("Démarrage automatique à l'heure prévue"), 8000);
}

void MainWindow::onWriteFailed(const QString& path, const QString& reason)
{
    statusBar()->showMessage(tr("Écriture impossible : %1 (%2)").arg(path, reason), 10000);

    // Un seul avertissement bloquant par session : en direct, une boite de
    // dialogue qui reapparait toutes les 100 ms serait ingerable.
    if (!m_writeErrorShown) {
        m_writeErrorShown = true;
        QMessageBox::warning(this, tr("Problème d'écriture"),
                             tr("TimeOverlay n'arrive pas à écrire dans :\n%1\n\n%2\n\n"
                                "OBS n'affichera plus de valeurs à jour tant que le problème "
                                "persiste. Vérifiez le dossier de sortie dans les paramètres.")
                                 .arg(path, reason));
    }
}

// --------------------------------------------------------------- affichage

void MainWindow::setBigDisplay(const QString& text, const QColor& color)
{
    m_bigDisplay->setText(text);
    m_bigDisplay->setStyleSheet(
        QStringLiteral("font-size: 96px; font-weight: bold; color: %1;").arg(color.name()));
}

void MainWindow::refreshDisplay()
{
    const bool hasProgramme = !m_profile.phases.isEmpty();
    m_btnStartPause->setEnabled(hasProgramme);
    m_btnNext->setEnabled(hasProgramme);
    m_btnPrevious->setEnabled(hasProgramme);
    m_btnAddMinute->setEnabled(hasProgramme);
    m_btnRemoveMinute->setEnabled(hasProgramme);
    m_btnReset->setEnabled(hasProgramme);

    if (!hasProgramme) {
        setBigDisplay(QStringLiteral("00:00:00"), m_colors.normalColor());
        m_statusLabel->setText(tr("Aucun programme chargé"));
    }
}
