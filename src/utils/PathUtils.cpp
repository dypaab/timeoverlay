#include "PathUtils.h"
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>

namespace {

// Noms reserves par Windows : creer "CON.txt" echoue ou ouvre un peripherique.
const QSet<QString>& reservedNames()
{
    static const QSet<QString> names = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    return names;
}

constexpr int kMaxNameLength = 64;

} // namespace

namespace PathUtils {

QString sanitizeName(const QString& name)
{
    QString out;
    out.reserve(name.size());

    for (const QChar c : name) {
        // Les lettres accentuees sont acceptees : "Prédication" comme
        // "Louange" doivent rester lisibles dans le nom de fichier.
        if (c.isLetterOrNumber() || c == '-' || c == '_' || c == ' ') {
            out.append(c);
        } else {
            out.append('_');
        }
    }

    // Espaces multiples et bords : evite "Chant  du  matin " -> fichiers douteux.
    out = out.simplified();

    // Un point en tete ou en fin pose probleme sur Windows comme sur Linux.
    while (out.startsWith('.') || out.startsWith('_')) out.remove(0, 1);
    while (out.endsWith('.') || out.endsWith('_')) out.chop(1);
    out = out.trimmed();

    if (out.size() > kMaxNameLength) {
        out.truncate(kMaxNameLength);
        out = out.trimmed();
    }

    if (out.isEmpty()) {
        return QStringLiteral("sans-nom");
    }

    // Un nom reserve doit etre neutralise meme avec une extension ajoutee
    // ensuite : Windows regarde la partie avant le premier point.
    const QString base = out.section('.', 0, 0).toUpper();
    if (reservedNames().contains(base)) {
        out.prepend('_');
    }

    return out;
}

bool isInside(const QDir& base, const QString& candidatePath)
{
    const QString canonicalBase = QFileInfo(base.absolutePath()).absoluteFilePath();
    const QString canonicalCandidate = QFileInfo(candidatePath).absoluteFilePath();

    // QDir::cleanPath resout les ".." textuels ; on compare ensuite les
    // prefixes en s'assurant que la frontiere tombe sur un separateur,
    // pour que "/base-autre" ne passe pas pour un enfant de "/base".
    const QString cleanBase = QDir::cleanPath(canonicalBase);
    const QString cleanCandidate = QDir::cleanPath(canonicalCandidate);

    if (cleanCandidate == cleanBase) return true;

    QString prefix = cleanBase;
    if (!prefix.endsWith('/')) prefix.append('/');

#ifdef Q_OS_WIN
    return cleanCandidate.startsWith(prefix, Qt::CaseInsensitive);
#else
    return cleanCandidate.startsWith(prefix, Qt::CaseSensitive);
#endif
}

QString csvEscape(const QString& field)
{
    QString out = field;

    // Injection de formules : un champ commencant par un de ces caracteres
    // est interprete comme une formule par les tableurs.
    if (!out.isEmpty()) {
        const QChar first = out.at(0);
        if (first == '=' || first == '+' || first == '-' || first == '@'
            || first == '\t' || first == '\r') {
            out.prepend('\'');
        }
    }

    if (out.contains('"') || out.contains(',') || out.contains('\n') || out.contains('\r')) {
        out.replace('"', "\"\"");
        out.prepend('"');
        out.append('"');
    }

    return out;
}

} // namespace PathUtils
