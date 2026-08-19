#include "TextFormatter.h"
#include <QtGlobal>

bool TextFormat::operator==(const TextFormat& other) const
{
    return maxCharsPerLine == other.maxCharsPerLine
        && maxLines == other.maxLines
        && overflow == other.overflow
        && uppercase == other.uppercase;
}

QString TextFormatter::wrap(const QString& text, int maxCharsPerLine)
{
    if (maxCharsPerLine <= 0) return text;

    QStringList outputLines;

    // Les retours a la ligne saisis par l'operateur sont respectes : chaque
    // paragraphe est replie independamment.
    const QStringList paragraphs = text.split('\n');
    for (const QString& paragraph : paragraphs) {
        const QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
        if (words.isEmpty()) {
            outputLines.append(QString());
            continue;
        }

        QString current;
        for (const QString& word : words) {
            // Mot plus long qu'une ligne entiere : on le decoupe, sinon il
            // depasserait de l'ecran quoi qu'on fasse.
            if (word.size() > maxCharsPerLine) {
                if (!current.isEmpty()) {
                    outputLines.append(current);
                    current.clear();
                }
                int position = 0;
                while (position < word.size()) {
                    const QString chunk = word.mid(position, maxCharsPerLine);
                    position += maxCharsPerLine;
                    if (position >= word.size()) {
                        current = chunk;  // le reste amorce la ligne suivante
                    } else {
                        outputLines.append(chunk);
                    }
                }
                continue;
            }

            if (current.isEmpty()) {
                current = word;
            } else if (current.size() + 1 + word.size() <= maxCharsPerLine) {
                current += ' ' + word;
            } else {
                outputLines.append(current);
                current = word;
            }
        }

        if (!current.isEmpty()) outputLines.append(current);
    }

    return outputLines.join('\n');
}

QString TextFormatter::limitLines(const QString& text, int maxLines)
{
    if (maxLines <= 0) return text;

    QStringList lines = text.split('\n');
    if (lines.size() <= maxLines) return text;

    lines = lines.mid(0, maxLines);

    // Signale visuellement que le message est tronque, pour que l'operateur
    // s'en apercoive a l'ecran de controle.
    QString& last = lines.last();
    last = last.trimmed();
    if (!last.endsWith(QStringLiteral("..."))) {
        last += QStringLiteral("...");
    }

    return lines.join('\n');
}

int TextFormatter::scrollCycleLength(const QString& text, const QString& separator)
{
    return text.size() + separator.size();
}

QString TextFormatter::scrollWindow(const QString& text, int width, int offset,
                                    const QString& separator)
{
    if (text.isEmpty()) return QString();
    if (width <= 0) return text;

    // Texte assez court pour tenir : aucun defilement, il resterait immobile
    // en donnant l'impression d'un bug.
    if (text.size() <= width) return text;

    const QString loop = text + separator;
    const int cycle = loop.size();
    if (cycle <= 0) return text;

    // Modulo protege des offsets negatifs.
    int start = offset % cycle;
    if (start < 0) start += cycle;

    QString window;
    window.reserve(width);
    for (int i = 0; i < width; ++i) {
        window.append(loop.at((start + i) % cycle));
    }
    return window;
}

QString TextFormatter::apply(const QString& text, const TextFormat& format)
{
    QString result = text;

    if (format.uppercase) {
        result = result.toUpper();
    }

    if (format.overflow == TextFormat::Overflow::Scroll) {
        // Le defilement se fait sur une seule ligne : les retours a la ligne
        // sont aplatis. La fenetre glissante est calculee ailleurs, en
        // fonction de la position courante.
        result.replace('\n', ' ');
        return result.simplified();
    }

    result = wrap(result, format.maxCharsPerLine);
    result = limitLines(result, format.maxLines);
    return result;
}
