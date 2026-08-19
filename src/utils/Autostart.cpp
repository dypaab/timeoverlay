#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

// Chemin a inscrire dans la configuration de demarrage.
//
// applicationFilePath() ne convient pas tel quel pour une AppImage : elle est
// montee dans un dossier temporaire (/tmp/.mount_XXXX) qui n'existera plus au
// prochain demarrage. Le lanceur d'AppImage expose le chemin reel du fichier
// dans la variable APPIMAGE -- c'est celui-la qu'il faut enregistrer.
QString executablePath()
{
    const QByteArray appImage = qgetenv("APPIMAGE");
    if (!appImage.isEmpty()) {
        const QString path = QString::fromLocal8Bit(appImage);
        if (QFileInfo::exists(path)) return path;
    }
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

#ifdef Q_OS_WIN

const char* kRunKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* kValueName = "TimeOverlay";

#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

QString desktopFilePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) configDir = QDir::home().filePath(QStringLiteral(".config"));
    return QDir(configDir).filePath(QStringLiteral("autostart/TimeOverlay.desktop"));
}

#endif

} // namespace

namespace Autostart {

bool isSupported()
{
#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    return true;
#else
    return false;
#endif
}

bool isEnabled()
{
#if defined(Q_OS_WIN)
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    return !run.value(QLatin1String(kValueName)).toString().isEmpty();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    return QFileInfo::exists(desktopFilePath());
#else
    return false;
#endif
}

bool setEnabled(bool enabled, QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& reason) {
        if (errorMessage) *errorMessage = reason;
        return false;
    };
    if (errorMessage) errorMessage->clear();

#if defined(Q_OS_WIN)
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        // Les guillemets sont indispensables : le chemin passe par
        // "Program Files", et sans eux Windows lance "C:\Program".
        run.setValue(QLatin1String(kValueName),
                     QStringLiteral("\"%1\"").arg(executablePath()));
    } else {
        run.remove(QLatin1String(kValueName));
    }
    run.sync();
    if (run.status() != QSettings::NoError) {
        return fail(QObject::tr("Le registre Windows a refusé l'écriture."));
    }
    return true;

#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString path = desktopFilePath();

    if (!enabled) {
        if (!QFileInfo::exists(path)) return true;
        QFile file(path);
        if (!file.remove()) return fail(file.errorString());
        return true;
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return fail(QObject::tr("Impossible de créer %1.").arg(QFileInfo(path).absolutePath()));
    }

    const QString contents = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=TimeOverlay\n"
        "Comment=Chronométrage du culte pour OBS\n"
        "Exec=\"%1\"\n"
        "Icon=TimeOverlay\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n").arg(executablePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return fail(file.errorString());
    if (file.write(contents.toUtf8()) == -1) return fail(file.errorString());
    if (!file.commit()) return fail(file.errorString());
    return true;

#else
    Q_UNUSED(enabled)
    return fail(QObject::tr("Non pris en charge sur ce système."));
#endif
}

} // namespace Autostart
