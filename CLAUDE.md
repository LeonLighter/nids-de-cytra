# CLAUDE.md

Tower defense à routes fixes dans l'univers d'Elternos, sur Unreal Engine 5, en C++.
Titre de travail : Les Nids de Cytra.

Le but actuel : reproduire dans Unreal le prototype web validé, tel qu'il est, ni plus ni moins.

## À lire avant de coder

- `Docs/GDD.md` : la spécification. C'est la source de vérité pour les règles et les valeurs.
- `Docs/prototype/nids-de-cytra.html` : le prototype web validé. Toute la logique de jeu est entre `/*LOGIC-START*/` et `/*LOGIC-END*/`, sans dépendance à l'affichage. En cas de doute sur un comportement, lis-la.
- Si le GDD et le prototype se contredisent, ne tranche pas seul : signale l'écart et demande.

## Environnement

- Version du moteur : 5.8 (`EngineAssociation`), build 5.8.4 installé depuis le launcher
- Dossier du moteur : `E:\Unreal Engines\UE_5.8` (absent du registre Windows, qui ne liste que 5.6 et 5.7)
- Projet : `D:\Projet Unreal\NidsDeCytra\NidsDeCytra.uproject`, module de jeu `NidsDeCytra`, cibles `NidsDeCytra` et `NidsDeCytraEditor`
- Système : Windows 11 Pro, Visual Studio 2022 (MSVC 14.44)
- Gestion de versions : Git avec Git LFS, dépôt privé `https://github.com/LeonLighter/nids-de-cytra`, branche principale `main`
- Plugins : Diversion désactivé (le projet est sur Git) ; `MCPClientToolset` et `ModelContextProtocol` actifs pour le serveur `unreal-mcp`

## Commandes (testées au jalon J00)

Les lancer depuis PowerShell : appelé depuis Git Bash, `Build.bat` ne fait rien et ne renvoie rien.

Compiler l'éditeur (éditeur fermé) :

```
& "E:\Unreal Engines\UE_5.8\Engine\Build\BatchFiles\Build.bat" NidsDeCytraEditor Win64 Development -Project="D:\Projet Unreal\NidsDeCytra\NidsDeCytra.uproject" -WaitMutex
```

Succès : `Result: Succeeded` et code de sortie 0.

Lancer les tests d'automatisation sans affichage :

```
& "E:\Unreal Engines\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Projet Unreal\NidsDeCytra\NidsDeCytra.uproject" -ExecCmds="Automation RunTest Cytra;Quit" -unattended -nopause -nosplash -nullrhi -log -stdout
```

La forme `Automation RunTest` fonctionne en 5.8. Lire le résultat dans la sortie ou dans `Saved/Logs/NidsDeCytra.log` :

- `Found N automation tests based on 'Cytra'` puis la liste des tests ;
- une ligne `Test Completed. Result={Success}` (ou `{Fail}`) par test ;
- `**** TEST COMPLETE. EXIT CODE: 0 ****` si tout est vert (code de sortie du processus : 0).

Deux lignes `LogAutomationTest: Error: Condition failed` apparaissent au démarrage, avant tout test : elles viennent de l'autotest interne du moteur (`UE::UnifiedErrorTest`), pas du projet. Les ignorer.

Lancer le jeu sans affichage, charger L_Passe01 et quitter (pour lire les lignes `LogCytra` dans `Saved/Logs/NidsDeCytra.log`) :

```
& "E:\Unreal Engines\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Projet Unreal\NidsDeCytra\NidsDeCytra.uproject" -game -nullrhi -unattended -nopause -nosplash -log -stdout -ExecCmds="Quit"
```

Pour vérifier à l'écran une fin de vague ou la victoire avant les tourelles, la commande console de développement `Cytra.KillAll` (absente des versions Shipping) abat tous les ennemis présents sur la carte, en versant leur or.

Le test `Cytra.Rules.Match.FullGameVsPrototype` joue une partie scriptée (file d'ordres, lancements de vague à heure fixe) et compare toutes les 30 secondes l'or, les œufs, la vague, les ennemis abattus, l'or dépensé et extrait, les tourelles, l'équipe et les chantiers aux valeurs du prototype. Ces valeurs de référence ont été obtenues en faisant jouer la même partie à la logique du prototype dans Chrome sans affichage (`chrome --headless=new --dump-dom`). Un changement d'équilibrage dans `Balance.json` ou les vagues demande de les recalculer de la même façon.

Bruit connu, sans rapport avec le projet : `LogPython: Error` venant de `ToolsetRegistry/.../init_unreal.py` (plugin expérimental du moteur), et `FindPlayerStart: ... NO PLAYERSTART` (le niveau n'a pas de PlayerStart, la caméra est placée par le GameMode).

Git et GitHub :

```
git status                                  # ce qui a changé
git switch -c jalon-20-ennemis              # crée la branche du jalon et bascule dessus
git add -A && git commit -m "J20 Message"   # committe une étape qui compile
git lfs ls-files                            # vérifie que les .uasset et .umap passent par Git LFS
git push -u origin jalon-20-ennemis         # pousse la branche sur GitHub
gh pr create --base main --title "J20 · Ennemis et vagues" --body-file <fichier>   # ouvre la pull request
```

## Règles de travail

1. **Jamais d'asset binaire.** Ne crée ni ne modifie de `.uasset` ou de `.umap`. Si un asset est indispensable, arrête-toi et dis-moi précisément quoi créer dans l'éditeur : type, nom, dossier, réglages.
2. **C++ d'abord.** Le jeu doit être jouable sans aucun Blueprint. Les visuels sont les formes de `/Engine/BasicShapes/` avec des matériaux dynamiques colorés. Je remplacerai par de vrais assets plus tard.
3. **Données hors du code.** Carte, vagues, ennemis, tourelles, spécialités et coûts vivent dans des fichiers texte sous `Content/Data/`. Les ennemis et les spécialités vont changer : en ajouter un ne doit demander qu'une ligne de données, pas une nouvelle classe.
4. **Règles séparées de l'affichage.** La logique (grille, accessibilité, chemins, économie, chantiers, vagues) est en classes C++ pures et déterministes, testables sans monde ni acteur. Les acteurs affichent l'état et relaient les ordres.
5. **Compile et teste après chaque changement.** Rien n'est « terminé » sans compilation verte et tests verts. Dis toujours clairement ce que tu n'as pas pu vérifier, en particulier tout ce qui se juge à l'écran.
6. **Éditeur fermé pour compiler.** Si la compilation échoue parce que l'éditeur est ouvert (Live Coding actif), demande-moi de le fermer. Ne contourne pas. Une nouvelle `UCLASS`, `USTRUCT` ou `UFUNCTION` demande un redémarrage de l'éditeur.
7. **Git et GitHub.** Dépôt privé, branche principale `main`. Tu ne pousses jamais directement sur `main`. Jamais de `push --force`, de `reset --hard`, de `clean`, de `rebase` ni de réécriture d'historique sans que je le demande.
8. **Une branche et une pull request par jalon.** Au début d'un jalon, crée sa branche `jalon-XX-nom` depuis `main` à jour (par exemple `jalon-20-ennemis`). Commits petits et fréquents, un par étape qui compile. À la fin du jalon, pousse la branche et ouvre une pull request vers `main` avec `gh pr create`, en mettant le résumé de session dans sa description. Ne fusionne pas la pull request : c'est moi qui fusionne après avoir vérifié le jalon en jeu.
9. **Fichiers exclus et Git LFS.** `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` et `.vs/` sont exclus par le `.gitignore`. Les `.uasset`, `.umap`, modèles, images et sons passent par Git LFS selon le `.gitattributes`. Avant un commit qui ajoute un binaire, vérifie avec `git lfs ls-files` qu'il est bien suivi par LFS. Un fichier déjà commité n'est plus ignoré : signale-moi tout fichier généré entré par erreur dans le dépôt.
10. **Rien d'ajouté en douce.** Pas de nouveau plugin, module ou dépendance sans mon accord.
11. **Un jalon à la fois.** Présente un plan avant d'écrire, attends ma validation, ne déborde pas sur le jalon suivant.
12. **Résumé de session.** Termine chaque jalon par ce résumé, que je recopie dans Notion :

```
Jalon : J?? · nom
Fait :
Vérifié (compilation, tests) :
À vérifier à l'écran par moi :
Non vérifié ou risqué :
Commit : branche, dernier commit, lien de la pull request
Suite proposée :
```

## Hors périmètre tant que je ne le demande pas

Chasseurs (tours à pièges, tireurs d'élite), tours de soutien des ingénieurs, forgeron et améliorations d'équipe, autres cartes, sauvegarde, son, multijoueur, art final. Le GDD les liste en « Prévu pour la suite » : l'architecture ne doit pas les empêcher, mais on ne les code pas.

## Conventions

- Code, noms et commentaires de code en anglais. Textes affichés au joueur en français, regroupés en `FText` (table de chaînes) pour rester traduisibles.
- Préfixe des types du jeu : `Cytra` (`ACytraGameMode`, `UCytraJobSubsystem`, `FCytraGrid`).
- Standard de code d'Epic : PascalCase, préfixes `A`/`U`/`F`/`E`/`I`, `UPROPERTY` sur tout pointeur vers un `UObject`, includes minimaux.
- Pas de `Tick` quand un timer ou un événement suffit.
- Catégorie de journal : `LogCytra`.
- Tests d'automatisation sous le préfixe `Cytra.` (par exemple `Cytra.Rules.Grid.ReachAtStart`).
- Messages de commit en français, à l'impératif, préfixés du jalon : `J20 Ajoute le déplacement des ennemis`.

## Architecture de départ (à affiner dans le plan du jalon J00)

- `Source/<Nom>/Rules/` : logique pure. Définition de carte, grille et accessibilité, recherche de chemin (parcours en largeur, 4 voisins), économie, file de chantiers, calendrier des vagues, équilibrage chargé depuis les données.
- `Source/<Nom>/Game/` : `ACytraGameMode` (déroulé : préparation, vague, victoire, défaite), `ACytraGameState` (or, œufs, vague, phase, compte à rebours), `ACytraPlayerController` (souris, sélection d'une case, ordres), `ACytraCameraPawn` (vue isométrique orthographique depuis le sud-ouest, zoom à la molette vers le curseur, déplacement ZQSD ou WASD une fois zoomé ; le calcul de vue, `FCytraIsoView`, est séparé de l'affichage et testé sans monde).
- `Source/<Nom>/World/` : `ACytraBoard` (génère le plateau depuis la carte, en instanced static meshes), `ACytraEnemy`, `ACytraWorker`, `ACytraTower`, `ACytraShots` (tous les projectiles et effets brefs, en instanced static meshes), `ACytraNest`.
- `Source/<Nom>/UI/` : interface écrite en C++ (Slate, ou UMG construit en C++), sans Widget Blueprint.
- `Source/<Nom>/Tests/` : tests d'automatisation.
- `Content/Data/` : carte, vagues et équilibrage en JSON, apparence provisoire des ennemis, des ouvriers et des tourelles (`Looks.json` ; une tourelle y a un socle `base`, une pièce `top` et `aims` si cette pièce pivote vers la cible ; le même fichier règle la caméra, section `camera` : inclinaison, orientation, marge, zoom maximal, pas de la molette, vitesse de déplacement, et la hauteur de chaque type de case, section `tileHeights`), textes affichés (`Text/CytraText.csv`, table de chaînes). Ce dossier est ajouté aux dossiers non-assets à empaqueter. Une spécialité, un chantier, une tourelle, un ennemi, une route ou un nom écrit sur la carte ajouté aux données demande aussi ses textes dans `CytraText.csv` (`Role.<Id>.Short`, `.Spec` et `.Name` pour une spécialité, `Work.<Id>.Label`, `.Ordered` et `.Done` pour un chantier, `Tower.<Id>.Name`, `.Desc`, `.Ordered` pour une tourelle, `Enemy.<Id>.One` et `.Many` pour un ennemi, `Route.<Id>.Name` pour une route, `Label.<Id>` pour un nom de la carte, placé par `labels` dans le fichier de la carte) et son apparence dans `Looks.json` : les tests `Cytra.UI.Text.DataKeys` et `Cytra.World.Looks.LoadsLooks` signalent ceux qui manquent. Une tourelle ne demande pas de code tant qu'elle reprend un comportement existant (`Bolt`, `Shell` ou `Chain` dans `Balance.json`).
- Entrées : Enhanced Input, actions et contexte créés en C++.
- Vitesse ×1, ×2, ×3 et pause gérées par le GameMode, comme dans le prototype : chaque image avance la simulation de son temps plafonné à 0,1 s puis multiplié par la vitesse, et de rien en pause. Le moteur n'est ni mis en pause ni ralenti : l'interface, les clics et les ordres restent actifs en pause. « Recommencer » recharge le niveau en gardant la vitesse (option d'URL `CytraSpeed`).
- Messages brefs, annonce de la vague et écran de fin : textes construits à part de l'affichage (`FCytraMessages`, `FCytraHudView`, `FCytraEndView`), testés sans widget. Les noms de la carte et l'or gagné (« +6 ») sont écrits par l'interface, à la position que la caméra donne aux points de la carte.

## Jalons

Les numéros vont de 10 en 10 et chaque phase a sa centaine : phase 1 (reproduire le prototype) de J00 à J90, phase 2 (nouvelles mécaniques) de J100 à J190, phase 3 (contenu) à partir de J200, phase 4 (finitions) à partir de J300. Un jalon ajouté plus tard prend un numéro libre entre deux, par exemple J25. Le suivi des jalons est tenu dans Notion ; quand j'ajoute un jalon, j'ajoute aussi sa ligne ici.

- **J00 · Socle.** Environnement détecté, commandes vérifiées, `.gitignore` et `.gitattributes`, arborescence, premier test, classes de base du jeu, plateau de 24 × 14 cases généré depuis les données.
- **J10 · Règles pures.** Grille, accessibilité, chaîne d'accès, chemins, économie, vagues, menace et temps de préparation, avec les tests de référence de la section 13 du GDD.
- **J20 · Ennemis et vagues.** Ennemis sur les deux routes, nid et œufs, victoire et défaite, déroulé des vagues, bonus de lancement anticipé, affichage minimal (or, œufs, vague, lancer la vague).
- **J30 · Ouvriers et chantiers.** Sélection d'une case à la souris, panneau d'ordres, ouvriers, déblayage, pont, échelle, avant-poste, retour au camp le plus proche.
- **J40 · Tourelles.** Baliste, mortier, bobine, tirs, améliorations, démontage.
- **J50 · Mine et recrutement.** Mine d'or, recrutement.
- **J60 · Interface complète.** Panneau d'équipe, messages, pause, vitesse, écran de fin. Partie entière comparée au prototype.
- **J65 · Vue isométrique.** Caméra orthographique en isométrie vraie depuis le sud-ouest, toute la carte visible, zoom à la molette, déplacement ZQSD ou WASD une fois zoomé, clic sur le premier bloc touché, réglages de caméra et hauteurs des cases dans `Looks.json`.
- **J66 · Préparation d'un playtest.** Changements éventuels à apporter avant de faire jouer des joueurs, à définir.
