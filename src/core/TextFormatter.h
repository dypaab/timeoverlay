#pragma once
#include <QString>
#include <QStringList>

// Mise en forme d'un texte pour un affichage donne.
//
// Le meme message part vers des supports tres differents : un bandeau d'une
// seule ligne en bas de l'image, un panneau de plusieurs lignes sur l'ecran
// de retour de l'orateur, un ecran d'assemblee plus large. Ces reglages
// permettent d'adapter le rendu sans retaper le message.
struct TextFormat {
    // Nombre de caracteres par ligne avant retour automatique.
    // 0 : aucun retour, le texte sort tel quel.
    int maxCharsPerLine = 0;

    // Nombre de lignes maximum. 0 : illimite.
    int maxLines = 0;

    enum class Overflow {
        Truncate,  // coupe et termine par des points de suspension
        Scroll     // fait defiler le texte horizontalement sur une ligne
    };
    Overflow overflow = Overflow::Truncate;

    // Majuscules forcees : utile sur les bandeaux ou la casse melangee
    // passe mal a distance.
    bool uppercase = false;

    bool operator==(const TextFormat& other) const;
    bool operator!=(const TextFormat& other) const { return !(*this == other); }
};

class TextFormatter
{
public:
    // Applique tous les reglages. Pour Overflow::Scroll, utiliser plutot
    // scrollWindow() qui prend la position du defilement en compte.
    static QString apply(const QString& text, const TextFormat& format);

    // Coupe le texte en lignes d'au plus maxCharsPerLine caracteres, sans
    // couper les mots. Un mot plus long qu'une ligne est coupe de force,
    // sinon il depasserait quand meme.
    static QString wrap(const QString& text, int maxCharsPerLine);

    // Limite le nombre de lignes et signale la coupe.
    static QString limitLines(const QString& text, int maxLines);

    // Fenetre glissante pour un texte defilant : renvoie les `width`
    // caracteres commencant a `offset`, en bouclant. Le separateur evite que
    // la fin du texte colle a son debut.
    static QString scrollWindow(const QString& text, int width, int offset,
                                const QString& separator = QStringLiteral("   -   "));

    // Longueur totale du cycle de defilement, pour savoir quand l'offset
    // doit revenir a zero.
    static int scrollCycleLength(const QString& text,
                                 const QString& separator = QStringLiteral("   -   "));
};
