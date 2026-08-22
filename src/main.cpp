#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // organizationName est volontairement laisse vide : QStandardPaths
    // compose AppDataLocation en "<AppData>/<organisation>/<application>",
    // et comme les deux valaient "TimeOverlay", le dossier de sortie
    // devenait %APPDATA%/TimeOverlay/TimeOverlay/obs. Sans organisation, on
    // obtient %APPDATA%/TimeOverlay/obs sous Windows et
    // ~/.local/share/TimeOverlay/obs sous Linux, ce qu'annoncent le README
    // et le script OBS.
    // QSettings n'est pas concerne : il est toujours construit avec son
    // organisation et son application explicites.
    app.setApplicationName(QStringLiteral("TimeOverlay"));
    // La version vient du CMakeLists : une seule source de verite, sinon le
    // paquet et l'option --version finissent par diverger.
    app.setApplicationVersion(QStringLiteral(TIMEOVERLAY_VERSION));

    // Icone embarquee dans le binaire : elle suit l'application quel que soit
    // le mode de distribution (paquet Debian, AppImage, dossier autonome).
    app.setWindowIcon(QIcon(QStringLiteral(":/TimeOverlay.png")));

    // Traductions de Qt lui-meme : sans elles, les boutons standard des
    // boites de dialogue restent en anglais ("Cancel", "Save"...) alors que
    // tout le reste de l'interface est en francais.
    //
    // On essaie plusieurs emplacements et plusieurs catalogues : selon que
    // l'application tourne depuis un paquet autonome ou depuis un Qt installe
    // par la distribution, les fichiers .qm ne sont ni au meme endroit ni
    // nommes pareil.
    {
        const QStringList directories = {
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath)
        };
        const QStringList catalogues = { QStringLiteral("qtbase"), QStringLiteral("qt") };

        for (const QString& directory : directories) {
            bool installed = false;
            for (const QString& catalogue : catalogues) {
                auto* translator = new QTranslator(&app);
                if (translator->load(QLocale::system(), catalogue,
                                     QStringLiteral("_"), directory)) {
                    app.installTranslator(translator);
                    installed = true;
                    break;
                }
                delete translator;
            }
            if (installed) break;
        }
    }

    // Une seule instance a la fois.
    //
    // Deux instances ecrivent dans le meme dossier de sortie et s'ecrasent
    // mutuellement : OBS affiche alors des valeurs incoherentes qui sautent.
    // C'est exactement le defaut que le moteur de sortie corrige a l'interieur
    // d'un processus, et qu'un second processus reintroduirait. Un double-clic
    // de trop un dimanche matin suffit.
    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (lockDir.isEmpty()) lockDir = QDir::tempPath();
    QDir().mkpath(lockDir);

    QLockFile lockFile(QDir(lockDir).filePath(QStringLiteral("TimeOverlay.lock")));
    // Un verrou plus vieux que 30 s dont le processus a disparu est considere
    // comme abandonne : un plantage ne doit pas empecher de relancer.
    lockFile.setStaleLockTime(30000);

    if (!lockFile.tryLock(300)) {
        qint64 pid = 0;
        QString host;
        QString application;
        lockFile.getLockInfo(&pid, &host, &application);

        QMessageBox::warning(
            nullptr, QObject::tr("TimeOverlay est déjà ouvert"),
            QObject::tr("TimeOverlay tourne déjà sur cet ordinateur%1.\n\n"
                        "Deux copies écriraient dans les mêmes fichiers et OBS "
                        "afficherait des valeurs incohérentes. Utilisez la fenêtre "
                        "déjà ouverte.")
                .arg(pid > 0 ? QObject::tr(" (processus %1)").arg(pid) : QString()));
        return 1;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QObject::tr("Chronométrage pour la diffusion : écrit l'heure, un compte à "
                    "rebours et un chronomètre dans des fichiers texte lus par OBS."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QObject::tr("programme"),
        QObject::tr("Fichier .timerproject à ouvrir au démarrage (facultatif)."));
    parser.process(app);

    MainWindow window;
    window.show();

    // Ouvert apres show() pour qu'un eventuel message d'erreur s'affiche
    // au-dessus d'une fenetre deja visible.
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        // Un fichier passe en argument est une intention explicite : elle
        // l'emporte sur une reprise de seance.
        window.openProgrammeFile(positional.first());
    } else {
        window.proposeSessionRestore();
    }

    return app.exec();
}
