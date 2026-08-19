#include "CSVExporter.h"
#include "PathUtils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QObject>

namespace {

QString formatDuration(int seconds)
{
    const bool negative = seconds < 0;
    const int abs = negative ? -seconds : seconds;
    const QString text = QStringLiteral("%1:%2:%3")
        .arg(abs / 3600, 2, 10, QChar('0'))
        .arg((abs % 3600) / 60, 2, 10, QChar('0'))
        .arg(abs % 60, 2, 10, QChar('0'));
    return negative ? QStringLiteral("-") + text : text;
}

} // namespace

bool CSVExporter::exportSession(const QString& path,
                                const QVector<SessionEntry>& entries,
                                QString* errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // BOM UTF-8 : sans lui, Excel affiche les accents de travers.
    out << QChar(0xFEFF);
    out << "Début,Phase,Prévu,Réel,Écart\n";

    for (const SessionEntry& e : entries) {
        out << PathUtils::csvEscape(e.startedAt.toString("yyyy-MM-dd HH:mm:ss")) << ','
            << PathUtils::csvEscape(e.phaseName) << ','
            << PathUtils::csvEscape(formatDuration(e.plannedSeconds)) << ','
            << PathUtils::csvEscape(formatDuration(e.actualSeconds)) << ','
            << PathUtils::csvEscape(formatDuration(e.deltaSeconds())) << '\n';
    }

    if (!file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    if (errorMessage) errorMessage->clear();
    return true;
}

bool CSVExporter::appendLog(const QString& logDir,
                            const QString& name,
                            const QString& event,
                            const QString& value,
                            QString* errorMessage)
{
    QDir dir(logDir);
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        if (errorMessage) *errorMessage = QObject::tr("dossier de journal introuvable");
        return false;
    }

    const QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const QString fileName = QStringLiteral("%1_%2.csv")
        .arg(PathUtils::sanitizeName(name), date);
    const QString path = dir.filePath(fileName);

    // Le nom a ete assaini, mais on verifie tout de meme que le chemin
    // resultant reste dans le dossier de journal.
    if (!PathUtils::isInside(dir, path)) {
        if (errorMessage) *errorMessage = QObject::tr("chemin de journal invalide");
        return false;
    }

    const bool needsHeader = !QFileInfo::exists(path);

    // Ajout en fin de fichier : QSaveFile ne convient pas ici, il remplace
    // le fichier entier.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    if (needsHeader) {
        out << QChar(0xFEFF);
        out << "Date,Heure,Objet,Événement,Valeur\n";
    }

    const QDateTime now = QDateTime::currentDateTime();
    out << PathUtils::csvEscape(now.toString("yyyy-MM-dd")) << ','
        << PathUtils::csvEscape(now.toString("HH:mm:ss")) << ','
        << PathUtils::csvEscape(name) << ','
        << PathUtils::csvEscape(event) << ','
        << PathUtils::csvEscape(value) << '\n';

    out.flush();
    file.close();

    if (file.error() != QFile::NoError) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    if (errorMessage) errorMessage->clear();
    return true;
}
