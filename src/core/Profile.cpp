#include "Profile.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QObject>
#include <QtGlobal>

namespace {

// "01:30:00", "90:00" et "5400" sont tous acceptes : les programmes sont
// souvent edites a la main.
int parseDuration(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double raw = value.toDouble();
        if (raw < 0 || raw > Phase::kMaxDurationSeconds) return 0;
        return int(raw);
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) return 0;

    const QStringList parts = text.split(':');
    if (parts.size() > 3) return 0;

    qint64 seconds = 0;
    for (const QString& part : parts) {
        bool ok = false;
        const int component = part.trimmed().toInt(&ok);
        if (!ok || component < 0) return 0;
        seconds = seconds * 60 + component;
        if (seconds > Phase::kMaxDurationSeconds) return Phase::kMaxDurationSeconds;
    }

    // Les trois arguments doivent avoir exactement le meme type, sinon la
    // resolution de surcharge de qBound est ambigue.
    return int(qBound(qint64(0), seconds, qint64(Phase::kMaxDurationSeconds)));
}

QString formatDuration(int seconds)
{
    seconds = qBound(0, seconds, Phase::kMaxDurationSeconds);
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds % 3600) / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

} // namespace

QJsonObject Phase::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["duration"] = formatDuration(durationSeconds);
    obj["overtimeEnabled"] = overtimeEnabled;
    return obj;
}

Phase Phase::fromJson(const QJsonObject& obj)
{
    Phase p;
    p.name = obj.value("name").toString().trimmed();
    if (p.name.isEmpty()) p.name = QObject::tr("Phase sans nom");

    p.durationSeconds = parseDuration(obj.value("duration"));

    // Absent du fichier : on garde le comportement historique (depassement
    // actif), qui correspond a l'usage attendu.
    const QJsonValue overtime = obj.value("overtimeEnabled");
    p.overtimeEnabled = overtime.isBool() ? overtime.toBool() : true;

    return p;
}

int Profile::totalDuration() const
{
    // Accumulation en 64 bits puis borne : additionner des int pouvait
    // deborder, ce qui est un comportement indefini en C++.
    qint64 total = 0;
    for (const Phase& p : phases) {
        total += p.durationSeconds;
        if (total > Phase::kMaxDurationSeconds) return Phase::kMaxDurationSeconds;
    }
    return int(total);
}

QTime Profile::startTimeAsTime() const
{
    if (startTime.trimmed().isEmpty()) return QTime();
    // "10:00" et "10:00:00" sont acceptes : les deux se rencontrent dans des
    // fichiers ecrits a la main.
    QTime parsed = QTime::fromString(startTime.trimmed(), QStringLiteral("HH:mm"));
    if (!parsed.isValid()) {
        parsed = QTime::fromString(startTime.trimmed(), QStringLiteral("HH:mm:ss"));
    }
    return parsed;
}

QJsonObject Profile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    if (!startTime.isEmpty()) {
        obj["startTime"] = startTime;
        obj["autoStart"] = autoStart;
    }
    QJsonArray arr;
    for (const Phase& p : phases) arr.append(p.toJson());
    obj["phases"] = arr;
    return obj;
}

Profile Profile::fromJson(const QJsonObject& obj)
{
    Profile prof;
    prof.name = obj.value("name").toString().trimmed();

    // Une heure malformee est ignoree plutot que refusee : le reste du
    // programme reste utilisable, seul le demarrage programme est perdu.
    prof.startTime = obj.value("startTime").toString().trimmed();
    if (!prof.startTime.isEmpty() && !prof.startTimeAsTime().isValid()) {
        prof.startTime.clear();
    }
    prof.autoStart = obj.value("autoStart").toBool(false) && !prof.startTime.isEmpty();

    const QJsonArray arr = obj.value("phases").toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        prof.phases.append(Phase::fromJson(v.toObject()));
        if (prof.phases.size() >= kMaxPhases) break;
    }

    if (prof.name.isEmpty() && !prof.phases.isEmpty()) {
        prof.name = QObject::tr("Programme sans nom");
    }
    return prof;
}

Profile Profile::fromFile(const QString& path, QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return Profile();
    };

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return fail(QObject::tr("Le fichier n'existe pas."));
    }
    if (info.size() > kMaxFileSizeBytes) {
        return fail(QObject::tr("Fichier trop volumineux pour un programme (%1 Ko).")
                        .arg(info.size() / 1024));
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QObject::tr("Lecture impossible : %1").arg(file.errorString()));
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(QObject::tr("Fichier JSON invalide : %1").arg(parseError.errorString()));
    }
    if (!doc.isObject()) {
        return fail(QObject::tr("Le fichier ne contient pas un programme valide."));
    }

    Profile prof = fromJson(doc.object());
    if (prof.phases.isEmpty()) {
        return fail(QObject::tr("Le programme ne contient aucune phase."));
    }

    if (errorMessage) errorMessage->clear();
    return prof;
}

bool Profile::saveToFile(const QString& path, QString* errorMessage) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    const QJsonDocument doc(toJson());
    if (file.write(doc.toJson(QJsonDocument::Indented)) == -1) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    if (errorMessage) errorMessage->clear();
    return true;
}
