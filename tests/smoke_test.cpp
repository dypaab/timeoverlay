// Test de fumee : verifie sans interface graphique les comportements dont
// depend la regie.
//
// Le point le plus important est le passage automatique en montee quand le
// compte a rebours atteint zero, et le fait que l'heure et le compte a
// rebours s'ecrivent dans des fichiers separes sans s'effacer mutuellement.
//
// Lancement : TimeOverlaySmokeTest
// Code de sortie 0 si tout passe, 1 sinon.

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#include "core/Timer.h"
#include "core/OutputEngine.h"
#include "core/Profile.h"
#include "core/TextFormatter.h"
#include "core/ServiceSchedule.h"
#include "core/ProgrammeLibrary.h"
#include "core/PhaseManager.h"
#include "core/SessionState.h"
#include "utils/PathUtils.h"

static int g_failures = 0;

static void check(bool condition, const QString& label, const QString& detail = QString())
{
    QTextStream out(stdout);
    if (condition) {
        out << "  OK   " << label << "\n";
    } else {
        ++g_failures;
        out << "  ECHEC " << label;
        if (!detail.isEmpty()) out << "  -> " << detail;
        out << "\n";
    }
    out.flush();
}

static QString readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QStringLiteral("<absent>");
    return QString::fromUtf8(file.readAll());
}

// ------------------------------------------------------- tests synchrones

static void testPathSecurity()
{
    QTextStream(stdout) << "\n[Securite des chemins]\n";

    check(!PathUtils::sanitizeName("../../evil").contains(".."),
          "la traversee de repertoire est neutralisee",
          PathUtils::sanitizeName("../../evil"));
    check(!PathUtils::sanitizeName("a/b\\c").contains('/')
              && !PathUtils::sanitizeName("a/b\\c").contains('\\'),
          "les separateurs de chemin sont supprimes");
    check(PathUtils::sanitizeName("CON") != QStringLiteral("CON"),
          "les noms de peripheriques Windows sont neutralises");
    check(PathUtils::sanitizeName("") == QStringLiteral("sans-nom"),
          "un nom vide reste utilisable");
    check(PathUtils::sanitizeName("Predication") == QStringLiteral("Predication"),
          "un nom normal est preserve");
}

static void testCsvEscaping()
{
    QTextStream(stdout) << "\n[Echappement CSV]\n";

    check(PathUtils::csvEscape("=SOMME(A1:A9)").startsWith('\''),
          "l'injection de formule est neutralisee",
          PathUtils::csvEscape("=SOMME(A1:A9)"));
    check(PathUtils::csvEscape("Louange, chant").startsWith('"'),
          "un champ contenant une virgule est entoure de guillemets");
    check(PathUtils::csvEscape("il a dit \"bonjour\"").contains("\"\""),
          "les guillemets internes sont doubles");
}

static void testProfileValidation()
{
    QTextStream(stdout) << "\n[Validation des programmes]\n";

    QJsonObject phase;
    phase["name"] = "Predication";
    phase["duration"] = "00:45:00";
    check(Phase::fromJson(phase).durationSeconds == 2700,
          "une duree HH:MM:SS est lue correctement");

    QJsonObject broken;
    broken["name"] = "Casse";
    broken["duration"] = "pas une duree";
    check(Phase::fromJson(broken).durationSeconds == 0,
          "une duree illisible retombe a zero au lieu de rester indefinie");

    QJsonObject huge;
    huge["name"] = "Enorme";
    huge["duration"] = "9999:00:00";
    check(Phase::fromJson(huge).durationSeconds <= Phase::kMaxDurationSeconds,
          "une duree aberrante est bornee");

    Profile profile;
    for (int i = 0; i < 50; ++i) {
        Phase p;
        p.durationSeconds = Phase::kMaxDurationSeconds;
        profile.phases.append(p);
    }
    check(profile.totalDuration() > 0,
          "la duree totale ne deborde pas en negatif");
}

static void testTextFormatting()
{
    QTextStream(stdout) << "\n[Mise en forme des messages]\n";

    const QString source = QStringLiteral("Un enfant vous attend a l'accueil");

    const QString wrapped = TextFormatter::wrap(source, 12);
    bool allLinesFit = true;
    for (const QString& line : wrapped.split('\n')) {
        if (line.size() > 12) allLinesFit = false;
    }
    check(allLinesFit, "aucune ligne ne depasse la largeur demandee",
          QString(wrapped).replace('\n', QStringLiteral(" / ")));

    check(TextFormatter::limitLines("a\nb\nc\nd", 2).split('\n').size() == 2,
          "le nombre de lignes est limite");

    const QString window = TextFormatter::scrollWindow(source, 10, 3);
    check(window.size() == 10, "la fenetre de defilement fait la largeur demandee", window);

    check(TextFormatter::scrollWindow("court", 20, 5) == QStringLiteral("court"),
          "un texte plus court que l'affichage ne defile pas");
}

static void testDurationParsing()
{
    QTextStream(stdout) << "\n[Saisie des durees]\n";

    int seconds = -1;
    check(Timer::parseDuration(QStringLiteral("5"), &seconds) && seconds == 300,
          "un nombre seul est compris en minutes", QString::number(seconds));

    check(Timer::parseDuration(QStringLiteral("05:30"), &seconds) && seconds == 330,
          "MM:SS est lu correctement", QString::number(seconds));

    check(Timer::parseDuration(QStringLiteral("01:15:00"), &seconds) && seconds == 4500,
          "HH:MM:SS est lu correctement", QString::number(seconds));

    check(Timer::parseDuration(QStringLiteral("  10  "), &seconds) && seconds == 600,
          "les espaces autour de la saisie sont ignores");

    check(!Timer::parseDuration(QStringLiteral("abc"), &seconds),
          "une saisie non numerique est refusee");
    check(!Timer::parseDuration(QString(), &seconds),
          "une saisie vide est refusee");
    check(!Timer::parseDuration(QStringLiteral("-5"), &seconds),
          "une duree negative est refusee");
    check(!Timer::parseDuration(QStringLiteral("1:2:3:4"), &seconds),
          "plus de trois champs est refuse");
    check(!Timer::parseDuration(QStringLiteral("99999"), &seconds),
          "une duree depassant 24 h est refusee");
}

static Profile makeProfile(const QString& name)
{
    Profile profile;
    profile.name = name;

    Phase louange;
    louange.name = QStringLiteral("Louange");
    louange.durationSeconds = 1800;
    profile.phases.append(louange);

    Phase predication;
    predication.name = QStringLiteral("Predication");
    predication.durationSeconds = 2700;
    profile.phases.append(predication);

    return profile;
}

static void testProfileModification()
{
    QTextStream(stdout) << "\n[Detection des modifications]\n";

    // La fenetre principale se sert de cette comparaison pour savoir s'il faut
    // proposer d'enregistrer en quittant. Si elle cessait de voir un
    // changement de duree, l'operateur perdrait son travail sans un mot.
    const Profile original = makeProfile(QStringLiteral("Culte"));

    Profile identical = original;
    check(identical.toJson() == original.toJson(),
          "un programme non touche n'est pas vu comme modifie");

    Profile changed = original;
    changed.phases[1].durationSeconds += 300;
    check(changed.toJson() != original.toJson(),
          "changer la duree d'une phase est vu comme une modification");

    Profile renamed = original;
    renamed.phases[0].name = QStringLiteral("Chants");
    check(renamed.toJson() != original.toJson(),
          "renommer une phase est vu comme une modification");
}

static void testProgrammeLibrary()
{
    QTextStream(stdout) << "\n[Bibliotheque de programmes]\n";

    QTemporaryDir dir;
    ProgrammeLibrary library(dir.path());

    const auto entryNamed = [&library](const QString& name) {
        for (const ProgrammeLibrary::Entry& entry : library.entries()) {
            if (entry.name == name) return entry;
        }
        return ProgrammeLibrary::Entry();
    };

    const Profile profile = makeProfile(QStringLiteral("Culte du dimanche"));

    QString savedPath;
    QString error;
    check(library.save(profile, &savedPath, &error),
          "un programme cree est enregistre sans rien demander", error);
    check(QFile::exists(savedPath), "le fichier existe sur le disque", savedPath);
    check(library.contains(savedPath), "il est reconnu comme appartenant a la bibliotheque");

    const ProgrammeLibrary::Entry listed = entryNamed(profile.name);
    check(listed.phaseCount == 2, "il apparait dans la liste avec ses phases",
          QString::number(listed.phaseCount));
    check(listed.totalSeconds == 4500, "sa duree totale est celle du programme",
          QString::number(listed.totalSeconds));
    check(!listed.lastUsedAt.isValid(), "un programme jamais deroule n'a pas de date d'usage");

    // Le nom du programme vient de l'utilisateur : il ne doit jamais servir
    // tel quel a fabriquer un chemin.
    Profile hostile = makeProfile(QStringLiteral("../../evil"));
    QString hostilePath;
    library.save(hostile, &hostilePath, nullptr);
    check(!hostilePath.isEmpty() && PathUtils::isInside(QDir(dir.path()), hostilePath),
          "un nom de programme ne peut pas ecrire hors de la bibliotheque", hostilePath);

    // --- historique ---
    QVector<SessionEntry> log;
    SessionEntry entry;
    entry.startedAt = QDateTime::currentDateTime();
    entry.phaseName = QStringLiteral("Predication");
    entry.plannedSeconds = 2700;
    entry.actualSeconds = 3000;
    log.append(entry);

    check(library.appendSession(profile.name, log, &error),
          "une seance est archivee dans l'historique du programme", error);

    const QVector<ProgrammeLibrary::Session> sessions = library.sessions(profile.name);
    check(sessions.size() == 1, "la seance est relue depuis le disque",
          QString::number(sessions.size()));
    if (!sessions.isEmpty()) {
        check(sessions.first().deltaSeconds() == 300,
              "l'ecart entre prevu et reel est conserve",
              QString::number(sessions.first().deltaSeconds()));
        check(sessions.first().phases.size() == 1,
              "le detail phase par phase est conserve");
    }
    check(entryNamed(profile.name).lastUsedAt.isValid(),
          "la liste affiche desormais une date de derniere utilisation");

    check(library.sessions(QStringLiteral("Programme jamais deroule")).isEmpty(),
          "un programme sans historique renvoie une liste vide");

    // --- changement de nom ---
    {
        // Renommer puis enregistrer laissait deux programmes dans la
        // bibliotheque : l'ancien avec tout l'historique, le nouveau vide.
        Profile renamed = profile;
        renamed.name = QStringLiteral("Culte du dimanche soir");

        QString renamedPath;
        library.save(renamed, &renamedPath, nullptr);
        check(library.dropPreviousName(savedPath, renamed.name, &error),
              "l'ancien nom est abandonne apres un changement de nom", error);
        check(!QFile::exists(savedPath), "l'ancien fichier ne subsiste pas");
        check(QFile::exists(renamedPath), "le programme existe sous son nouveau nom");
        check(library.sessions(renamed.name).size() == 1,
              "l'historique a suivi le nouveau nom",
              QString::number(library.sessions(renamed.name).size()));
        check(library.sessions(profile.name).isEmpty(),
              "il ne reste rien sous l'ancien nom");

        // On remet le programme d'origine en place pour la suite du test.
        library.save(profile, &savedPath, nullptr);
        library.appendSession(profile.name, log, nullptr);
        library.remove(renamedPath, nullptr);
    }

    // --- suppression ---
    check(library.remove(savedPath, &error), "un programme se supprime", error);
    check(!QFile::exists(savedPath), "son fichier a disparu");
    check(library.sessions(profile.name).isEmpty(), "son historique part avec lui");
}

static void testSessionRecording()
{
    QTextStream(stdout) << "\n[Compte rendu de seance]\n";

    PhaseManager manager;
    manager.loadProfile(makeProfile(QStringLiteral("Culte")));
    manager.start();

    check(manager.sessionLog().isEmpty(),
          "une phase en cours n'est pas encore inscrite au compte rendu");

    manager.nextPhase();
    check(manager.sessionLog().size() == 1,
          "quitter une phase l'inscrit au compte rendu",
          QString::number(manager.sessionLog().size()));

    // Sans endSession(), la derniere phase du culte -- la predication -- ne
    // serait jamais archivee, puisqu'on ne la quitte pas.
    manager.endSession();
    check(manager.sessionLog().size() == 2,
          "la derniere phase est archivee a la fin de la seance",
          QString::number(manager.sessionLog().size()));

    manager.endSession();
    check(manager.sessionLog().size() == 2,
          "une seconde fin de seance n'archive pas la phase deux fois",
          QString::number(manager.sessionLog().size()));
}

static void waitMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void testJamaisLesDeuxEnsemble()
{
    QTextStream(stdout) << "\n[Compte a rebours et depassement : jamais ensemble]\n";

    // Les deux sources OBS sont posees au meme endroit de l'ecran. La regle
    // qu'elles imposent est absolue : a tout instant, une seule des deux
    // valeurs porte du texte. Ce test l'echantillonne de part et d'autre de
    // zero, y compris pendant la seconde du basculement.
    Timer timer(QStringLiteral("Exclusion"), 2);
    timer.start();

    int mesures = 0;
    int fautes = 0;
    int vuAvant = 0;
    int vuApres = 0;

    for (int i = 0; i < 35; ++i) {
        waitMs(100);
        const bool rebours = !timer.countdown().isEmpty();
        const bool depassement = !timer.overtime().isEmpty();

        ++mesures;
        if (rebours && depassement) ++fautes;   // superposition
        if (!rebours && !depassement) ++fautes; // trou noir a l'ecran
        if (rebours) ++vuAvant;
        if (depassement) ++vuApres;
    }

    check(fautes == 0,
          "sur 35 mesures, exactement une des deux valeurs est affichee",
          QStringLiteral("%1 faute(s) sur %2").arg(fautes).arg(mesures));
    check(vuAvant > 0, "le compte a rebours a bien ete vu avant zero");
    check(vuApres > 0, "le depassement a bien ete vu apres zero");
}

static void testRepriseApresFermeture()
{
    QTextStream(stdout) << "\n[Reprise apres une fermeture accidentelle]\n";

    QTemporaryDir dossier;
    const QString chemin = QDir(dossier.path()).filePath(QStringLiteral("session.json"));

    // --- une phase qui tournait : le culte a continue sans nous ---
    {
        SessionState session;
        session.profilePath = QStringLiteral("/un/programme.timerproject");
        session.profileName = QStringLiteral("Culte");
        session.phaseIndex = 2;
        session.phaseName = QStringLiteral("Predication");
        session.phaseDurationSeconds = 2700;
        // Phase demarree il y a 10 minutes, application fermee il y a 30 s.
        session.phaseStartedAt = QDateTime::currentDateTime().addSecs(-600);
        session.frozenElapsedSeconds = -1;
        session.savedAt = QDateTime::currentDateTime().addSecs(-30);

        QString erreur;
        check(session.save(chemin, &erreur), "l'etat de reprise s'enregistre", erreur);

        const SessionState relu = SessionState::load(chemin);
        check(relu.isValid(), "il se relit depuis le disque");
        check(relu.phaseIndex == 2 && relu.phaseName == QStringLiteral("Predication"),
              "la phase en cours est retrouvee");
        check(relu.phaseDurationSeconds == 2700,
              "la duree ajustee est conservee");
        check(relu.isRunning(), "la phase est reconnue comme en cours");

        // Le point central : le temps ecoule se recalcule sur l'horloge, il ne
        // reprend pas la ou l'enregistrement l'avait laisse.
        const int ecoule = relu.elapsedSecondsNow();
        check(ecoule >= 598 && ecoule <= 602,
              "le temps ecoule a continue de courir pendant l'absence",
              QStringLiteral("%1 s au lieu de ~600").arg(ecoule));
    }

    // --- une phase en pause : le temps devait rester fige ---
    {
        SessionState session;
        session.profilePath = QStringLiteral("/un/programme.timerproject");
        session.phaseIndex = 0;
        session.phaseDurationSeconds = 600;
        session.phaseStartedAt = QDateTime::currentDateTime().addSecs(-900);
        session.frozenElapsedSeconds = 120;   // suspendue a 2 minutes
        session.savedAt = QDateTime::currentDateTime().addSecs(-60);
        session.save(chemin, nullptr);

        const SessionState relu = SessionState::load(chemin);
        check(!relu.isRunning(), "une phase suspendue est reconnue comme telle");
        check(relu.elapsedSecondsNow() == 120,
              "son temps reste fige : le culte etait reellement arrete",
              QString::number(relu.elapsedSecondsNow()));
    }

    // --- une seance d'un autre jour ne doit pas etre proposee ---
    {
        // Le fichier est ecrit a la main : save() estampille toujours l'heure
        // courante, et c'est voulu. On ne peut donc pas fabriquer une vieille
        // seance en passant par lui.
        const QDateTime hier = QDateTime::currentDateTime().addSecs(-86400);
        const QString json = QStringLiteral(
            "{\"profilePath\":\"/un/programme.timerproject\",\"phaseIndex\":1,"
            "\"phaseStartedAt\":\"%1\",\"frozenElapsed\":-1,\"savedAt\":\"%1\"}")
            .arg(hier.toString(Qt::ISODate));

        QFile fichier(chemin);
        fichier.open(QIODevice::WriteOnly);
        fichier.write(json.toUtf8());
        fichier.close();

        const SessionState relu = SessionState::load(chemin);
        check(relu.isValid(), "la seance d'hier se relit correctement");
        check(relu.secondsSinceSaved() > SessionState::kMaxAgeSeconds,
              "une seance vieille d'un jour depasse le delai de reprise",
              QStringLiteral("%1 s").arg(relu.secondsSinceSaved()));
    }

    // --- un etat incomplet ne doit pas etre pris pour bon ---
    {
        SessionState::discard(chemin);
        check(!SessionState::load(chemin).isValid(),
              "un fichier absent ne donne pas d'etat valide");

        QFile fichier(chemin);
        fichier.open(QIODevice::WriteOnly);
        fichier.write("{ pas du json");
        fichier.close();
        check(!SessionState::load(chemin).isValid(),
              "un fichier corrompu ne donne pas d'etat valide");
    }
}

static void testMinuteurRepris()
{
    QTextStream(stdout) << "\n[Minuteur repris en cours de route]\n";

    // Phase de 10 s reprise a 4 s : il doit rester 6 s, pas 10.
    Timer timer(QStringLiteral("Repris"), 10);
    timer.resumeAt(4, true);
    check(timer.state() == Timer::State::RUNNING, "la phase repart en decompte");
    const int restant = timer.remainingSeconds();
    check(restant >= 5 && restant <= 6,
          "le temps restant tient compte du temps deja ecoule",
          QString::number(restant));

    // Zero franchi pendant l'absence : on revient directement en depassement.
    Timer depasse(QStringLiteral("Depasse"), 10);
    depasse.resumeAt(25, true);
    check(depasse.state() == Timer::State::OVERTIME,
          "une phase deja depassee revient en depassement");
    check(depasse.countdown().isEmpty(),
          "son compte a rebours est vide, comme il se doit");
    check(depasse.overtime() == QStringLiteral("-00:00:15"),
          "le depassement affiche le bon retard",
          depasse.overtime());

    // Une phase qui s'arrete net et dont l'heure etait passee : terminee.
    Timer nette(QStringLiteral("Nette"), 10);
    nette.setOvertimeEnabled(false);
    nette.resumeAt(25, true);
    check(nette.state() == Timer::State::FINISHED,
          "une phase sans depassement revient a l'etat termine");
}

static void testPhaseQuiSarreteNet()
{
    QTextStream(stdout) << "\n[Phase qui s'arrete net]\n";

    // Contrepartie de l'effacement du compte a rebours : une phase dont le
    // depassement est decoche n'a RIEN pour prendre le relais. Si elle
    // s'effacait aussi, l'ecran deviendrait vide au lieu d'annoncer la fin.
    Timer timer(QStringLiteral("Net"), 1);
    timer.setOvertimeEnabled(false);
    timer.start();
    waitMs(1600);

    check(timer.state() == Timer::State::FINISHED,
          "la phase s'arrete au lieu de basculer en depassement");
    check(timer.countdown() == QStringLiteral("00:00:00"),
          "elle garde son 00:00:00 : rien ne vient le remplacer",
          QStringLiteral("\"%1\"").arg(timer.countdown()));
    check(timer.overtime().isEmpty(),
          "aucun depassement n'est affiche",
          QStringLiteral("\"%1\"").arg(timer.overtime()));
}

static void testSchedule()
{
    QTextStream(stdout) << "\n[Heure de debut du culte]\n";

    QJsonObject withStart;
    withStart["name"] = "Culte";
    withStart["startTime"] = "10:00";
    withStart["autoStart"] = true;
    QJsonArray phases;
    QJsonObject phase;
    phase["name"] = "Louange";
    phase["duration"] = "00:30:00";
    phases.append(phase);
    withStart["phases"] = phases;

    const Profile parsed = Profile::fromJson(withStart);
    check(parsed.startTimeAsTime() == QTime(10, 0),
          "l'heure de debut est lue depuis le programme");
    check(parsed.autoStart, "le demarrage automatique est lu depuis le programme");

    QJsonObject broken = withStart;
    broken["startTime"] = "pas une heure";
    const Profile brokenProfile = Profile::fromJson(broken);
    check(!brokenProfile.startTimeAsTime().isValid(),
          "une heure malformee est ignoree");
    check(!brokenProfile.autoStart,
          "le demarrage automatique est desactive si l'heure est illisible");

    // QTime n'a pas de date : pres de minuit, "il y a une minute" repasse de
    // l'autre cote du cadran. On evite cette fenetre plutot que d'ecrire un
    // test qui echouerait une fois par nuit.
    const int hour = QTime::currentTime().hour();
    if (hour < 1 || hour > 22) {
        QTextStream(stdout) << "  (tests horaires sautes : trop pres de minuit)\n";
        return;
    }

    {
        // Heure deja passee au moment du reglage : rien ne doit demarrer.
        // C'est la garde qui empeche d'ouvrir le logiciel a 10h15 et de voir
        // le culte se lancer tout seul sur l'heure de la semaine passee.
        ServiceSchedule schedule;
        schedule.setAutoStartEnabled(true);
        bool fired = false;
        QObject::connect(&schedule, &ServiceSchedule::startTimeReached,
                         [&fired]() { fired = true; });
        schedule.setStartTime(QTime::currentTime().addSecs(-120));
        waitMs(700);
        check(!fired, "une heure deja passee ne declenche aucun demarrage");
        check(schedule.remainingText().isEmpty(),
              "aucun decompte affiche pour une heure passee");
    }

    {
        ServiceSchedule schedule;
        schedule.setStartTime(QTime::currentTime().addSecs(300));
        check(!schedule.remainingText().isEmpty(),
              "le decompte avant le debut est renseigne",
              schedule.remainingText());
    }

    {
        // Heure atteinte pendant que l'application tourne : declenchement.
        ServiceSchedule schedule;
        schedule.setAutoStartEnabled(true);
        bool fired = false;
        QObject::connect(&schedule, &ServiceSchedule::startTimeReached,
                         [&fired]() { fired = true; });
        schedule.setStartTime(QTime::currentTime().addSecs(2));
        check(!schedule.remainingText().isEmpty(), "le decompte court avant l'heure");
        waitMs(3200);
        check(fired, "l'heure atteinte declenche le demarrage");
        check(schedule.remainingText().isEmpty(),
              "le decompte se vide une fois le culte demarre");
    }

    {
        // Demarrage automatique desactive : le decompte fonctionne, mais rien
        // ne se lance.
        ServiceSchedule schedule;
        schedule.setAutoStartEnabled(false);
        bool fired = false;
        QObject::connect(&schedule, &ServiceSchedule::startTimeReached,
                         [&fired]() { fired = true; });
        schedule.setStartTime(QTime::currentTime().addSecs(1));
        waitMs(2200);
        check(!fired, "sans demarrage automatique, l'heure ne lance rien");
    }
}

// ------------------------------------------------- test du minuteur (async)

class TimerScenario : public QObject
{
    Q_OBJECT
public:
    TimerScenario(const QString& outputDir, QObject* parent = nullptr)
        : QObject(parent), m_output(new OutputEngine(this)), m_dir(outputDir)
    {
        m_output->setBaseDir(outputDir);

        // Deux secondes : assez court pour un test, assez long pour observer
        // le decompte puis la bascule.
        m_timer = new Timer(QStringLiteral("Test"), 2, this);

        connect(m_timer, &Timer::tick, this,
                [this](const QString& countdown, const QString& overtime, Timer::State) {
            m_output->set(OutputEngine::Countdown, countdown);
            m_output->set(OutputEngine::Countup, m_timer->countup());
            m_output->set(OutputEngine::Depassement, overtime);
        });

        connect(m_timer, &Timer::overtimeStarted, this, [this]() { m_overtimeSignalSeen = true; });
    }

    void run()
    {
        QTextStream(stdout) << "\n[Minuteur et fichiers de sortie]\n";

        // L'horloge ecrit sa propre cle, en parallele du minuteur : c'est
        // exactement le scenario qui etait casse avant.
        m_output->set(OutputEngine::Heure, QStringLiteral("10:42:07"));
        m_timer->start();

        QTimer::singleShot(1000, this, &TimerScenario::checkRunning);
        QTimer::singleShot(3500, this, &TimerScenario::checkOvertime);
        QTimer::singleShot(4000, this, &TimerScenario::finish);
    }

private slots:
    void checkRunning()
    {
        check(m_timer->state() == Timer::State::RUNNING,
              "le minuteur est en decompte apres 1 s");
        // Fourchette et non egalite stricte : QTimer peut se declencher
        // quelques millisecondes avant ou apres l'echeance, et l'affichage
        // arrondit au superieur. Exiger exactement 1 rendait le test instable
        // sans qu'aucun defaut reel ne soit en cause.
        const int remaining = m_timer->remainingSeconds();
        check(remaining >= 1 && remaining <= 2,
              "il reste environ 1 s sur un minuteur de 2 s apres 1 s",
              QString::number(remaining));
        check(m_timer->overtime().isEmpty(),
              "aucun depassement n'est signale avant zero");
    }

    void checkOvertime()
    {
        check(m_timer->state() == Timer::State::OVERTIME,
              "le minuteur est passe en depassement apres zero");
        check(m_overtimeSignalSeen,
              "le signal de debut de depassement a bien ete emis");
        // Dans OBS, le compte a rebours et le depassement sont souvent poses
        // au meme endroit. Un 00:00:00 fige restait affiche sous le temps de
        // depassement : le compte a rebours doit donc s'effacer.
        check(m_timer->countdown().isEmpty(),
              "le compte a rebours s'efface une fois zero franchi",
              QStringLiteral("\"%1\"").arg(m_timer->countdown()));
        check(m_timer->remainingSeconds() == 0,
              "le temps restant reste a zero et ne devient pas negatif",
              QString::number(m_timer->remainingSeconds()));
        check(m_timer->overtime().startsWith('-'),
              "le depassement est signe en negatif",
              m_timer->overtime());
        check(m_timer->elapsedSeconds() >= 3,
              "le temps ecoule continue de progresser au-dela de la duree prevue",
              QString::number(m_timer->elapsedSeconds()));
    }

    void finish()
    {
        m_output->flushNow();

        // Les noms de fichiers sont un contrat : ils sont cites dans le
        // README, dans le script OBS, et surtout dans les sources Texte deja
        // configurees par l'utilisateur. En renommer un casse silencieusement
        // une installation en production.
        QTextStream(stdout) << "\n[Noms des fichiers de sortie]\n";
        const QStringList expectedFiles = {
            QStringLiteral("heure.txt"),          QStringLiteral("date.txt"),
            QStringLiteral("countdown.txt"),      QStringLiteral("countup.txt"),
            QStringLiteral("depassement.txt"),
            QStringLiteral("statut.txt"),         QStringLiteral("phase.txt"),
            QStringLiteral("phase_suivante.txt"), QStringLiteral("message.txt"),
            QStringLiteral("annonce.txt"),        QStringLiteral("avant_debut.txt")
        };
        QStringList missing;
        for (const QString& name : expectedFiles) {
            if (!QFile::exists(m_dir + QLatin1Char('/') + name)) missing.append(name);
        }
        check(missing.isEmpty(),
              QStringLiteral("les %1 fichiers documentes portent le nom attendu")
                  .arg(expectedFiles.size()),
              missing.join(QStringLiteral(", ")));

        const QString heure = readFile(m_dir + "/heure.txt");
        const QString countdown = readFile(m_dir + "/countdown.txt");
        const QString depassement = readFile(m_dir + "/depassement.txt");
        const QString countup = readFile(m_dir + "/countup.txt");

        QTextStream(stdout) << "\n[Independance des fichiers]\n";
        check(heure == QStringLiteral("10:42:07"),
              "heure.txt n'a PAS ete efface par le minuteur", heure);
        check(countdown.isEmpty(),
              "countdown.txt est vide pendant le depassement, il ne se superpose pas",
              QStringLiteral("\"%1\"").arg(countdown));
        check(depassement.startsWith('-'),
              "depassement.txt contient le temps en trop, signe en negatif", depassement);
        check(!countup.isEmpty() && countup != QStringLiteral("00:00:00"),
              "countup.txt monte independamment", countup);

        QCoreApplication::quit();
    }

private:
    OutputEngine* m_output = nullptr;
    Timer* m_timer = nullptr;
    QString m_dir;
    bool m_overtimeSignalSeen = false;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCritical("Impossible de creer un dossier temporaire");
        return 1;
    }

    QTextStream(stdout) << "=== TimeOverlay - test de fumee ===\n";

    testPathSecurity();
    testCsvEscaping();
    testProfileValidation();
    testTextFormatting();
    testDurationParsing();
    testProfileModification();
    testProgrammeLibrary();
    testJamaisLesDeuxEnsemble();
    testRepriseApresFermeture();
    testMinuteurRepris();
    testPhaseQuiSarreteNet();
    testSessionRecording();
    testSchedule();

    TimerScenario scenario(tempDir.path());
    scenario.run();
    app.exec();

    QTextStream out(stdout);
    out << "\n===================================\n";
    if (g_failures == 0) {
        out << "TOUS LES TESTS PASSENT\n";
    } else {
        out << g_failures << " TEST(S) EN ECHEC\n";
    }
    out.flush();
    return g_failures == 0 ? 0 : 1;
}

#include "smoke_test.moc"
