#include "OutputEngine.h"
#include "../utils/PathUtils.h"
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>

const QString OutputEngine::Heure         = QStringLiteral("heure");
const QString OutputEngine::Date          = QStringLiteral("date");
const QString OutputEngine::Countdown     = QStringLiteral("countdown");
const QString OutputEngine::Countup       = QStringLiteral("countup");
// ATTENTION : ces cles donnent leur nom aux fichiers de sortie. Elles sont
// citees telles quelles dans le README, dans le script OBS et dans les sources
// Texte deja configurees par les utilisateurs. Elles restent sans accent et ne
// doivent jamais etre "corrigees" en meme temps que les libelles d'interface.
const QString OutputEngine::Depassement   = QStringLiteral("depassement");
const QString OutputEngine::Statut        = QStringLiteral("statut");
const QString OutputEngine::Phase         = QStringLiteral("phase");
const QString OutputEngine::PhaseSuivante = QStringLiteral("phase_suivante");
const QString OutputEngine::Message       = QStringLiteral("message");
const QString OutputEngine::Annonce       = QStringLiteral("annonce");
const QString OutputEngine::AvantDebut    = QStringLiteral("avant_debut");

QStringList OutputEngine::standardKeys()
{
    return { Heure, Date, Countdown, Countup, Depassement,
             Statut, Phase, PhaseSuivante, Message, Annonce, AvantDebut };
}

OutputEngine::OutputEngine(QObject *parent) : QObject(parent)
{
    // 10 Hz : assez fin pour un affichage fluide dans OBS, et comme on
    // n'ecrit que ce qui a change, une valeur au format HH:mm:ss ne
    // declenche qu'une seule ecriture par seconde.
    m_flushTimer.setInterval(100);
    connect(&m_flushTimer, &QTimer::timeout, this, &OutputEngine::flushNow);
    m_flushTimer.start();

    for (const QString& key : standardKeys()) {
        m_values.insert(key, QString());
    }
}

bool OutputEngine::setBaseDir(const QString& path)
{
    if (path.isEmpty()) return false;

    QDir dir(path);
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        emit writeFailed(dir.absolutePath(), tr("impossible de créer le dossier"));
        return false;
    }

    // Un dossier existant mais non inscriptible (cle USB en lecture seule,
    // droits perdus) doit etre detecte maintenant, pas au premier tick.
    const QFileInfo info(dir.absolutePath());
    if (!info.isWritable()) {
        emit writeFailed(dir.absolutePath(), tr("dossier non accessible en écriture"));
        return false;
    }

    m_baseDir = dir;
    m_hasBaseDir = true;
    m_reportedFailures.clear();
    writeAll();
    return true;
}

QString OutputEngine::pathFor(const QString& key) const
{
    // Les cles sont des constantes internes, mais on les assainit quand meme :
    // une cle personnalisee pourra etre ajoutee plus tard sans rouvrir un trou.
    return m_baseDir.filePath(PathUtils::sanitizeName(key) + QStringLiteral(".txt"));
}

void OutputEngine::set(const QString& key, const QString& value)
{
    if (m_values.value(key) == value && m_values.contains(key)) {
        return;
    }
    m_values.insert(key, value);
    m_dirty.insert(key);
}

void OutputEngine::writeAll()
{
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        m_dirty.insert(it.key());
    }
    flushNow();
}

void OutputEngine::flushNow()
{
    // Sans dossier valide, QDir pointerait sur le repertoire courant et on
    // disperserait des fichiers texte la ou l'application a ete lancee.
    if (!m_hasBaseDir) return;
    if (m_dirty.isEmpty()) return;

    const QSet<QString> pending = m_dirty;
    m_dirty.clear();

    for (const QString& key : pending) {
        const QString path = pathFor(key);

        // Filet de securite : meme si une cle malformee arrivait ici, elle
        // ne pourra pas ecrire hors du dossier de sortie.
        if (!PathUtils::isInside(m_baseDir, path)) {
            continue;
        }

        if (!writeAtomic(path, m_values.value(key))) {
            if (!m_reportedFailures.contains(path)) {
                m_reportedFailures.insert(path);
                emit writeFailed(path, tr("écriture impossible"));
            }
        }
    }
}

bool OutputEngine::writeAtomic(const QString& path, const QString& content)
{
    // QSaveFile ecrit dans un fichier temporaire puis renomme : OBS ne peut
    // jamais lire un fichier a moitie ecrit, ce qui provoquerait un
    // clignotement a l'antenne.
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        if (file.commit()) {
            return true;
        }
    }

    // Sur Windows, le renommage peut echouer si un autre programme tient le
    // fichier ouvert. On retombe alors sur une ecriture directe : moins sur
    // vis-a-vis des lectures partielles, mais mieux que ne rien afficher.
    QFile fallback(path);
    if (!fallback.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&fallback);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    fallback.close();
    return fallback.error() == QFile::NoError;
}
