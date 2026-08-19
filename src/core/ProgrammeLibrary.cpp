#include "ProgrammeLibrary.h"
#include "../utils/PathUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStandardPaths>

namespace {

const char* kExtension = ".timerproject";
const char* kHistoryFolder = "historique";

// Meme plafond que pour un programme : ces fichiers sont ecrits par
// l'application, mais rien n'empeche de les remplacer a la main.
constexpr qint64 kMaxHistoryBytes = 4 * 1024 * 1024;

QString isoDate(const QDateTime& moment)
{
    return moment.toString(Qt::ISODate);
}

} // namespace

ProgrammeLibrary::ProgrammeLibrary(const QString& directory) : m_dir(directory)
{
}

QString ProgrammeLibrary::defaultDirectory()
{
    // AppDataLocation contient deja le nom de l'application -- meme raison que
    // pour le dossier de sortie, ne pas le repeter.
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral("TimeOverlay"));
    }
    return QDir(base).filePath(QStringLiteral("programmes"));
}

QString ProgrammeLibrary::pathFor(const QString& programmeName) const
{
    const QDir dir(m_dir);
    const QString path = dir.filePath(PathUtils::sanitizeName(programmeName) + QLatin1String(kExtension));

    // Le nom a ete assaini, mais on verifie le chemin obtenu : c'est le
    // deuxieme filet, celui qui a deja rattrape une traversee de repertoire
    // ailleurs dans le projet.
    return PathUtils::isInside(dir, path) ? path : QString();
}

bool ProgrammeLibrary::contains(const QString& path) const
{
    if (path.isEmpty()) return false;

    // Comparaison des dossiers parents plutot que du prefixe : un fichier pose
    // dans un sous-dossier de la bibliotheque n'en fait pas partie.
    const QString parent = QDir::cleanPath(QFileInfo(path).absolutePath());
    return parent == QDir::cleanPath(QFileInfo(m_dir).absoluteFilePath());
}

QVector<ProgrammeLibrary::Entry> ProgrammeLibrary::entries() const
{
    QVector<Entry> result;

    QDir dir(m_dir);
    if (!dir.exists()) return result;

    const QFileInfoList files = dir.entryInfoList(
        { QStringLiteral("*%1").arg(QLatin1String(kExtension)) }, QDir::Files, QDir::Name);

    for (const QFileInfo& info : files) {
        const Profile profile = Profile::fromFile(info.absoluteFilePath());
        if (!profile.isValid()) continue;

        Entry entry;
        entry.path = info.absoluteFilePath();
        entry.name = profile.name.isEmpty() ? info.completeBaseName() : profile.name;
        entry.phaseCount = int(profile.phases.size());
        entry.totalSeconds = profile.totalDuration();
        entry.savedAt = info.lastModified();

        const QVector<Session> past = sessions(entry.name);
        entry.sessionCount = int(past.size());
        if (!past.isEmpty()) entry.lastUsedAt = past.last().startedAt;

        result.append(entry);
    }

    // Le programme le plus recemment deroule en tete : c'est presque toujours
    // celui qu'on veut rouvrir. Ceux qui n'ont jamais servi passent apres,
    // classes par date d'enregistrement.
    std::sort(result.begin(), result.end(), [](const Entry& a, const Entry& b) {
        if (a.lastUsedAt.isValid() != b.lastUsedAt.isValid()) return a.lastUsedAt.isValid();
        if (a.lastUsedAt.isValid()) return a.lastUsedAt > b.lastUsedAt;
        return a.savedAt > b.savedAt;
    });

    return result;
}

bool ProgrammeLibrary::save(const Profile& profile, QString* savedPath, QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return false;
    };

    if (!profile.isValid()) {
        return fail(QObject::tr("Le programme ne contient aucune phase."));
    }
    if (!QDir().mkpath(m_dir)) {
        return fail(QObject::tr("Impossible de créer le dossier %1.").arg(m_dir));
    }

    const QString path = pathFor(profile.name);
    if (path.isEmpty()) {
        return fail(QObject::tr("Le nom « %1 » ne donne pas un nom de fichier utilisable.")
                        .arg(profile.name));
    }

    QString error;
    if (!profile.saveToFile(path, &error)) return fail(error);

    if (savedPath) *savedPath = path;
    if (errorMessage) errorMessage->clear();
    return true;
}

bool ProgrammeLibrary::remove(const QString& path, QString* errorMessage)
{
    if (!contains(path)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Ce fichier n'appartient pas à la bibliothèque.");
        }
        return false;
    }

    // Le nom est lu AVANT la suppression : c'est lui qui designe le fichier
    // d'historique, et apres coup il n'est plus lisible.
    const QString programmeName = Profile::fromFile(path).name;

    QFile file(path);
    if (!file.remove()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    // L'historique du programme part avec lui : le garder sous un nom qui ne
    // correspond plus a rien le rendrait seulement impossible a retrouver.
    if (!programmeName.isEmpty()) QFile::remove(historyPath(programmeName));

    if (errorMessage) errorMessage->clear();
    return true;
}

bool ProgrammeLibrary::dropPreviousName(const QString& previousPath,
                                        const QString& newName,
                                        QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return false;
    };
    if (errorMessage) errorMessage->clear();

    if (!contains(previousPath) || !QFileInfo::exists(previousPath)) return true;
    if (QDir::cleanPath(previousPath) == QDir::cleanPath(pathFor(newName))) return true;

    const QString previousName = Profile::fromFile(previousPath).name;

    // L'historique demenage sous le nouveau nom. S'il en existe deja un
    // la-bas, on ne l'ecrase pas : celui du nom d'arrivee a la priorite, et
    // l'ancien reste consultable tant qu'on ne l'a pas supprime.
    if (!previousName.isEmpty()) {
        const QString from = historyPath(previousName);
        const QString to = historyPath(newName);
        if (!from.isEmpty() && !to.isEmpty()
            && QFileInfo::exists(from) && !QFileInfo::exists(to)) {
            if (!QFile::rename(from, to)) {
                return fail(QObject::tr("L'historique n'a pas pu suivre le nouveau nom."));
            }
        }
    }

    QFile previous(previousPath);
    if (!previous.remove()) return fail(previous.errorString());
    return true;
}

// ------------------------------------------------------------- historique

QString ProgrammeLibrary::historyPath(const QString& programmeName) const
{
    const QDir dir(QDir(m_dir).filePath(QLatin1String(kHistoryFolder)));
    const QString path = dir.filePath(PathUtils::sanitizeName(programmeName)
                                      + QStringLiteral(".json"));
    return PathUtils::isInside(dir, path) ? path : QString();
}

bool ProgrammeLibrary::appendSession(const QString& programmeName,
                                     const QVector<SessionEntry>& phases,
                                     QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return false;
    };

    if (phases.isEmpty()) return fail(QObject::tr("Aucune phase déroulée."));

    const QString path = historyPath(programmeName);
    if (path.isEmpty()) return fail(QObject::tr("Chemin d'historique invalide."));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return fail(QObject::tr("Impossible de créer le dossier d'historique."));
    }

    QJsonArray sessionArray;
    for (const Session& past : sessions(programmeName)) {
        QJsonObject obj;
        obj["startedAt"] = isoDate(past.startedAt);
        obj["planned"] = past.plannedSeconds;
        obj["actual"] = past.actualSeconds;

        QJsonArray phaseArray;
        for (const SessionEntry& phase : past.phases) {
            QJsonObject p;
            p["name"] = phase.phaseName;
            p["startedAt"] = isoDate(phase.startedAt);
            p["planned"] = phase.plannedSeconds;
            p["actual"] = phase.actualSeconds;
            phaseArray.append(p);
        }
        obj["phases"] = phaseArray;
        sessionArray.append(obj);
    }

    QJsonObject fresh;
    fresh["startedAt"] = isoDate(phases.first().startedAt);
    int planned = 0;
    int actual = 0;
    QJsonArray phaseArray;
    for (const SessionEntry& phase : phases) {
        planned += phase.plannedSeconds;
        actual += phase.actualSeconds;

        QJsonObject p;
        p["name"] = phase.phaseName;
        p["startedAt"] = isoDate(phase.startedAt);
        p["planned"] = phase.plannedSeconds;
        p["actual"] = phase.actualSeconds;
        phaseArray.append(p);
    }
    fresh["planned"] = planned;
    fresh["actual"] = actual;
    fresh["phases"] = phaseArray;
    sessionArray.append(fresh);

    while (sessionArray.size() > kMaxSessionsKept) sessionArray.removeFirst();

    QJsonObject root;
    root["programme"] = programmeName;
    root["sessions"] = sessionArray;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1) {
        return fail(file.errorString());
    }
    if (!file.commit()) return fail(file.errorString());

    if (errorMessage) errorMessage->clear();
    return true;
}

QVector<ProgrammeLibrary::Session> ProgrammeLibrary::sessions(const QString& programmeName) const
{
    QVector<Session> result;

    const QString path = historyPath(programmeName);
    if (path.isEmpty()) return result;

    const QFileInfo info(path);
    if (!info.exists() || info.size() > kMaxHistoryBytes) return result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return result;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return result;

    const QJsonArray sessionArray = doc.object().value("sessions").toArray();
    for (const QJsonValue& value : sessionArray) {
        if (!value.isObject()) continue;
        const QJsonObject obj = value.toObject();

        Session session;
        session.startedAt = QDateTime::fromString(obj.value("startedAt").toString(), Qt::ISODate);
        session.plannedSeconds = obj.value("planned").toInt();
        session.actualSeconds = obj.value("actual").toInt();

        const QJsonArray phaseArray = obj.value("phases").toArray();
        for (const QJsonValue& phaseValue : phaseArray) {
            if (!phaseValue.isObject()) continue;
            const QJsonObject p = phaseValue.toObject();

            SessionEntry phase;
            phase.phaseName = p.value("name").toString();
            phase.startedAt = QDateTime::fromString(p.value("startedAt").toString(), Qt::ISODate);
            phase.plannedSeconds = p.value("planned").toInt();
            phase.actualSeconds = p.value("actual").toInt();
            session.phases.append(phase);
        }

        result.append(session);
    }

    return result;
}
