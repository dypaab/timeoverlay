#pragma once
#include <QObject>
#include <QDir>
#include <QHash>
#include <QSet>
#include <QString>
#include <QTimer>

// Moteur de sortie central.
//
// Raison d'etre : chaque valeur affichable possede son propre fichier et sa
// propre entree d'etat. Un module qui met a jour l'heure ne touche jamais au
// countdown. C'est ce qui permet a OBS d'afficher l'heure, le countdown et le
// countup simultanement -- le comportement de Snaz sur lequel repose le
// fonctionnement en regie.
//
// L'ancienne implementation ecrivait les cinq fichiers OBS d'un seul bloc a
// chaque tick : l'horloge remettait le countdown a vide chaque seconde et
// reciproquement. Ne jamais revenir a une ecriture groupee.
class OutputEngine : public QObject
{
    Q_OBJECT
public:
    explicit OutputEngine(QObject *parent = nullptr);

    // Cles des sorties standard. Chacune correspond a un fichier <cle>.txt
    // dans le dossier de sortie, a pointer depuis une source Texte d'OBS.
    static const QString Heure;          // 10:42:07
    static const QString Date;           // 2026-08-16
    static const QString Countdown;      // 00:04:31  (temps restant)
    static const QString Countup;        // 00:12:05  (temps ecoule de la phase)
    static const QString Depassement;    // -00:01:22 (vide tant qu'on ne depasse pas)
    static const QString Statut;         // EN_COURS / PAUSE / DEPASSEMENT ...
    static const QString Phase;          // "Prédication"
    static const QString PhaseSuivante;  // "Annonces"
    static const QString Message;        // message de fin, ex "Merci de conclure"
    static const QString Annonce;        // ligne d'annonce rotative
    static const QString AvantDebut;     // 00:12:34 avant le debut, vide ensuite

    // Renvoie false si le dossier n'a pas pu etre cree ou n'est pas accessible
    // en ecriture. L'appelant doit prevenir l'utilisateur : sans dossier
    // valide, OBS n'affichera plus rien.
    bool setBaseDir(const QString& path);
    QString baseDir() const { return m_baseDir.absolutePath(); }

    // Met a jour une valeur. Le fichier n'est reecrit que si elle a change,
    // ce qui evite des centaines de milliers d'ecritures inutiles par jour.
    void set(const QString& key, const QString& value);
    void clear(const QString& key) { set(key, QString()); }
    QString value(const QString& key) const { return m_values.value(key); }

    // Ecrit immediatement tout ce qui est en attente, sans attendre le
    // prochain cycle. A utiliser a l'arret de l'application.
    void flushNow();

    // Reecrit tous les fichiers, meme inchanges. Utile apres un changement
    // de dossier de sortie : les nouveaux fichiers doivent exister tout de
    // suite pour qu'OBS ne pointe pas dans le vide.
    void writeAll();

    // Liste des cles standard, pour la documentation et l'interface.
    static QStringList standardKeys();

signals:
    // Emis au maximum une fois par chemin en echec, pour ne pas noyer
    // l'utilisateur si le disque est plein.
    void writeFailed(const QString& path, const QString& reason);

private:
    QDir m_baseDir;
    bool m_hasBaseDir = false;   // tant que faux, rien n'est ecrit sur disque
    QHash<QString, QString> m_values;
    QSet<QString> m_dirty;
    QSet<QString> m_reportedFailures;
    QTimer m_flushTimer;

    QString pathFor(const QString& key) const;
    bool writeAtomic(const QString& path, const QString& content);
};
