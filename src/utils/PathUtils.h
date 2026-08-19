#pragma once
#include <QString>
#include <QDir>

// Utilitaires de securite pour tout ce qui finit dans un chemin de fichier
// ou dans un CSV. Les noms d'objets sont saisis par l'utilisateur ou lus
// depuis un fichier de programme : ils ne doivent jamais etre concatenes
// tels quels dans un chemin.
namespace PathUtils {

// Rend un nom utilisable comme composant de nom de fichier.
// Supprime les separateurs, les sequences ".." et les caracteres interdits,
// refuse les noms de peripheriques reserves de Windows (CON, NUL, COM1...)
// et limite la longueur. Renvoie "sans-nom" si rien d'exploitable ne reste.
QString sanitizeName(const QString& name);

// Verifie qu'un chemin resolu reste bien a l'interieur du dossier de base.
// Deuxieme filet de securite apres sanitizeName.
bool isInside(const QDir& base, const QString& candidatePath);

// Echappe un champ pour ecriture CSV : guillemets doubles, virgules et
// retours a la ligne. Neutralise aussi l'injection de formules en prefixant
// d'une apostrophe les champs commencant par = + - @ (executes par Excel
// et LibreOffice a l'ouverture du fichier).
QString csvEscape(const QString& field);

} // namespace PathUtils
