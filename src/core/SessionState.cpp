#include "SessionState.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
// Meme plafond que pour un programme : ce fichier est ecrit par
// l'application, mais rien n'empeche de le remplacer a la main.
constexpr qint64 kMaxFileSizeBytes = 64 * 1024;
}

bool SessionState::isValid() const
{
    return phaseIndex >= 0
        && !profilePath.isEmpty()
        && phaseStartedAt.isValid()
        && savedAt.isValid();
}

int SessionState::elapsedSecondsNow() const
{
    if (!isRunning()) return frozenElapsedSeconds;
    if (!phaseStartedAt.isValid()) return 0;

    // L'horloge murale fait foi : le culte a continue pendant l'absence.
    const qint64 secondes = phaseStartedAt.secsTo(QDateTime::currentDateTime());
    return secondes < 0 ? 0 : int(secondes);
}

int SessionState::secondsSinceSaved() const
{
    if (!savedAt.isValid()) return 0;
    const qint64 secondes = savedAt.secsTo(QDateTime::currentDateTime());
    return secondes < 0 ? 0 : int(secondes);
}

QString SessionState::defaultPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::home().filePath(QStringLiteral("TimeOverlay"));
    return QDir(base).filePath(QStringLiteral("session.json"));
}

bool SessionState::save(const QString& path, QString* errorMessage) const
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return false;
    };

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return fail(QStringLiteral("dossier de session introuvable"));
    }

    QJsonObject obj;
    obj["profilePath"] = profilePath;
    obj["profileName"] = profileName;
    obj["phaseIndex"] = phaseIndex;
    obj["phaseName"] = phaseName;
    obj["phaseDuration"] = phaseDurationSeconds;
    obj["phaseStartedAt"] = phaseStartedAt.toString(Qt::ISODate);
    obj["frozenElapsed"] = frozenElapsedSeconds;
    obj["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Ecriture atomique : l'application peut disparaitre a n'importe quel
    // instant, y compris pendant cette ecriture. Un fichier a moitie ecrit
    // rendrait la reprise impossible -- exactement quand on en a besoin.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    if (file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact)) == -1) {
        return fail(file.errorString());
    }
    if (!file.commit()) return fail(file.errorString());

    if (errorMessage) errorMessage->clear();
    return true;
}

SessionState SessionState::load(const QString& path)
{
    SessionState etat;

    const QFileInfo info(path);
    if (!info.exists() || info.size() > kMaxFileSizeBytes) return etat;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return etat;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return etat;

    const QJsonObject obj = doc.object();
    etat.profilePath = obj.value("profilePath").toString();
    etat.profileName = obj.value("profileName").toString();
    etat.phaseIndex = obj.value("phaseIndex").toInt(-1);
    etat.phaseName = obj.value("phaseName").toString();
    etat.phaseDurationSeconds = obj.value("phaseDuration").toInt();
    etat.phaseStartedAt = QDateTime::fromString(obj.value("phaseStartedAt").toString(), Qt::ISODate);
    etat.frozenElapsedSeconds = obj.value("frozenElapsed").toInt(-1);
    etat.savedAt = QDateTime::fromString(obj.value("savedAt").toString(), Qt::ISODate);

    return etat;
}

void SessionState::discard(const QString& path)
{
    QFile::remove(path);
}
