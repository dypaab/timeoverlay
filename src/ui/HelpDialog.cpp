#include "HelpDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>

namespace {

// Un lien d'action porte le schema "app". Tout le reste (un chemin de dossier,
// une adresse) est confie au systeme.
const char* kActionScheme = "app";

QString page(const QString& outputDir)
{
    return QStringLiteral(R"(
<style>
  body     { font-size: 14px; }
  h2       { margin-top: 22px; margin-bottom: 4px; }
  h3       { margin-top: 14px; margin-bottom: 2px; font-size: 14px; }
  p, li    { line-height: 150%; }
  td       { padding-right: 18px; padding-bottom: 3px; }
  .chemin  { color: #555555; }
  .sommaire a { text-decoration: none; }
</style>

<h1>Utiliser TimeOverlay</h1>

<p>TimeOverlay écrit l'heure, le compte à rebours et le temps de dépassement
dans des <b>fichiers texte</b>. OBS lit ces fichiers et les affiche. Le logiciel
ne parle jamais directement à OBS : c'est ce qui lui permet de continuer à
fonctionner même si vous redémarrez OBS en plein culte.</p>

<p class="sommaire">
  <a href="#preparer">1. Préparer un programme</a> &nbsp;·&nbsp;
  <a href="#deroulement">2. Pendant le culte</a> &nbsp;·&nbsp;
  <a href="#obs">3. Brancher OBS</a> &nbsp;·&nbsp;
  <a href="#messages">4. Messages et annonces</a> &nbsp;·&nbsp;
  <a href="#overlay">5. L'affichage flottant</a> &nbsp;·&nbsp;
  <a href="#reglages">6. Réglages</a>
</p>

<hr>

<h2><a name="preparer"></a>1. Préparer un programme</h2>

<p>Un programme est le déroulé du culte : accueil, louange, annonces,
prédication, avec la durée prévue de chacun.</p>

<ul>
  <li><a href="app:nouveau">Créer un nouveau programme</a> — il est
      <b>enregistré tout seul</b> dans votre bibliothèque dès que vous validez.
      Vous n'avez pas de fichier à choisir ni de dossier à retenir.</li>
  <li><a href="app:programmes">Ouvrir la bibliothèque</a> — la liste de tous
      vos programmes, avec pour chacun l'historique des cultes où il a servi :
      durée prévue, durée réelle, écart.</li>
  <li><a href="app:modifier">Modifier le programme en cours</a> — changer une
      durée, renommer une phase, en ajouter ou en retirer, changer l'ordre.
      À la fermeture de l'application, TimeOverlay vous demandera si vous
      voulez enregistrer vos modifications.</li>
</ul>

<h3>L'heure de début</h3>

<p>Dans l'éditeur, cochez <i>« Le culte commence à »</i> pour afficher un
décompte avant le début, à mettre sur l'écran d'accueil. Si vous cochez aussi
<i>« démarrer la première phase automatiquement »</i>, elle se lancera toute
seule à l'heure dite — mais seulement elle. Les phases suivantes se lancent
toujours à la main, pour qu'une phase qui déborde ne soit jamais coupée.</p>

<h2><a name="deroulement"></a>2. Pendant le culte</h2>

<table>
  <tr><td><b>Démarrer / Pause</b></td>
      <td>lance ou suspend la phase en cours</td></tr>
  <tr><td><b>Suivant &gt;</b></td>
      <td>passe à la phase suivante, même si la précédente n'est pas finie</td></tr>
  <tr><td><b>+ 1 min / - 1 min</b></td>
      <td>ajuste la phase en cours sans toucher au programme enregistré</td></tr>
  <tr><td><b>Remise à zéro</b></td>
      <td>revient au début du programme</td></tr>
  <tr><td><b>Minuteur libre</b></td>
      <td>un compte à rebours ponctuel, hors programme</td></tr>
</table>

<p>Quand le compte à rebours atteint zéro, il <b>ne s'arrête pas</b> : le temps
en trop est compté à la hausse dans un fichier séparé. Rien n'est jamais coupé
automatiquement — c'est vous qui décidez.</p>

<p>Le minuteur libre et le programme écrivent dans les mêmes fichiers, pour
qu'OBS n'ait pas à être reconfiguré. Ils ne peuvent donc pas tourner en même
temps : démarrer l'un met l'autre en pause, et la barre d'état vous le dit.</p>

<h3>Si le logiciel se ferme par accident</h3>

<p>Rouvrez-le : il propose de <b>reprendre là où vous en étiez</b>, avec le
programme, la phase et le temps écoulé. Et le temps a bien <b>continué de
courir</b> pendant l'absence — le prédicateur n'a pas fait de pause parce
qu'une fenêtre s'est fermée. Une phase qui était en pause, elle, garde son
temps figé.</p>

<p>Rien ne redémarre tout seul : la question vous est posée, et
« Repartir de zéro » reste à un clic. Passé deux heures, la proposition
n'apparaît plus.</p>

<h2><a name="obs"></a>3. Brancher OBS</h2>

<p>Dans OBS, ajoutez une source <b>Texte (GDI+)</b>, cochez
<i>« Lire depuis un fichier »</i> et pointez-la sur le fichier voulu.</p>

<ul>
  <li><a href="app:chemins">Voir et copier les chemins des fichiers</a> —
      la fenêtre reste ouverte, vous copiez les chemins un par un.</li>
  <li><a href="app:dossier">Ouvrir le dossier de sortie</a></li>
</ul>

<p class="chemin">Dossier actuel : %1</p>

<h3>Superposer le compte à rebours et le dépassement</h3>

<p><code>depassement.txt</code> reste <b>vide</b> tant que la phase ne déborde
pas, et une source Texte vide n'affiche rien dans OBS. Créez donc une source en
rouge pointant dessus : elle restera invisible tout le culte et apparaîtra
seule au moment du dépassement, sans script ni condition à écrire.</p>

<p>Au même instant, <code>countdown.txt</code> <b>se vide</b>. Vous pouvez donc
poser les deux sources exactement au même endroit de l'écran : elles se
relaient, sans jamais se superposer. Le dépassement s'affiche en négatif —
<code>-00:01:22</code>.</p>

<p>Deux exceptions à connaître. Une phase réglée pour <b>s'arrêter net</b>
garde son <code>00:00:00</code> : rien ne viendrait le remplacer. Et si une de
vos scènes n'affiche <i>que</i> le compte à rebours, elle deviendra vide en cas
de dépassement — pointez-y plutôt <code>countup.txt</code>, qui ne se vide
jamais.</p>

<p>Même principe d'effacement pour <code>avant_debut.txt</code>, qui se vide
dès que le culte démarre.</p>

<h2><a name="messages"></a>4. Messages et annonces</h2>

<p>Le panneau de droite écrit dans deux fichiers distincts :</p>

<ul>
  <li><code>message.txt</code> — un mot ponctuel à l'intervenant
      (« on termine », « un enfant vous attend à l'accueil »). Il reste affiché
      tant que vous ne l'effacez pas : un changement de phase ne l'enlève pas
      sous vos pieds.</li>
  <li><code>annonce.txt</code> — plusieurs annonces qui défilent à tour de
      rôle. La rotation est <b>à l'arrêt</b> au démarrage de l'application ;
      c'est à vous de la lancer.</li>
</ul>

<h2><a name="overlay"></a>5. L'affichage flottant</h2>

<p><a href="app:overlay">Afficher ou masquer l'overlay</a> — une petite fenêtre
posée par-dessus les autres, qui montre l'heure et le compte à rebours. Elle
sert de retour à l'intervenant quand on ne passe pas par OBS, typiquement sur
un second écran tourné vers la scène.</p>

<p><b>Elle se déplace en la faisant glisser à la souris</b>, n'importe où sur sa
surface. Sa position est retenue d'une ouverture à l'autre. Sur un poste à deux
écrans, elle s'ouvre d'office au centre du second.</p>

<h2><a name="reglages"></a>6. Réglages</h2>

<p><a href="app:parametres">Ouvrir les paramètres</a> pour régler :</p>

<ul>
  <li>le dossier où sont écrits les fichiers lus par OBS ;</li>
  <li>le format de l'heure et de la date ;</li>
  <li>à partir de combien de secondes le compte à rebours passe à l'orange puis
      au rouge, et les couleurs elles-mêmes ;</li>
  <li>le son de fin de phase ;</li>
  <li>le <b>lancement au démarrage de l'ordinateur</b>. Il est actif : comme
      l'application affiche l'heure et le décompte avant le culte, elle doit
      déjà être ouverte quand vous arrivez à la régie. Décochez cette case si
      vous préférez la lancer vous-même.</li>
</ul>

<h3>Une seule fenêtre à la fois</h3>

<p>TimeOverlay refuse de s'ouvrir deux fois. Deux copies écriraient dans les
mêmes fichiers et OBS afficherait des valeurs incohérentes. Si l'application
semble déjà lancée sans être visible, cherchez-la dans la barre des tâches
avant de double-cliquer à nouveau.</p>
)").arg(outputDir);
}

} // namespace

HelpDialog::HelpDialog(const QString& outputDir, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Aide - TimeOverlay"));
    resize(760, 680);

    auto* root = new QVBoxLayout(this);

    m_browser = new QTextBrowser(this);
    // Les liens sont interceptes : ceux du sommaire naviguent dans la page,
    // ceux en "app:" declenchent une action de la fenetre principale.
    m_browser->setOpenLinks(false);
    m_browser->setOpenExternalLinks(false);
    m_browser->setHtml(page(outputDir));
    root->addWidget(m_browser);

    connect(m_browser, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.scheme() == QLatin1String(kActionScheme)) {
            emit actionRequested(url.path());
            return;
        }
        // Ancre interne : QTextBrowser ne suit plus les liens tout seul, il
        // faut lui redemander explicitement de se deplacer.
        if (!url.fragment().isEmpty()) {
            m_browser->scrollToAnchor(url.fragment());
            return;
        }
        QDesktopServices::openUrl(url);
    });

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* btnClose = new QPushButton(tr("Fermer"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(btnClose);
    root->addLayout(buttons);
}
