# Les Nids de Cytra : spécification du jeu

État : conforme au prototype web validé (`Docs/prototype/nids-de-cytra.html`), version du 9 octobre 2026.
Toutes les valeurs sont provisoires et doivent rester modifiables dans les données.

## 1. Résumé

Cytra, bête divine du foyer et mère de toutes les araignées, est trop gigantesque pour protéger tous ses nids. Le Conclave de Lothering (nains et gnomes) envoie ses ouvriers garder les œufs d'un nid dans une passe de montagne.

- Tower defense à routes fixes, vue de dessus, un joueur.
- Le joueur ne construit rien lui-même. Il donne des ordres payants ; des ouvriers spécialisés se déplacent et les exécutent.
- Beaucoup d'emplacements de tourelle sont inaccessibles au départ. Il faut d'abord ouvrir l'accès : déblayer, jeter un pont, poser une échelle.
- Une partie dure 10 vagues. Le joueur perd si les 12 œufs sont détruits.

## 2. Carte « La passe »

Grille de 24 colonnes × 14 lignes. `x` va de 0 (ouest) à 23 (est), `y` de 0 (nord) à 13 (sud). Les distances de ce document sont en cases.

Convention proposée dans Unreal : 1 case = 200 unités ; `x` de la grille suit +Y du monde, `y` de la grille suit −X du monde, pour que la carte apparaisse comme ci-dessous dans une vue de dessus.

```
     012345678901234567890123
  0  ########################
  1  ########################
  2  ###o####################
  3  .......LH###############
  4  ######.####o############
  5  #####,............######
  6  #####,###G#######.#o####
  7  ####P,##o,o##oLH#.....N#
  8  #####,#######R###.##,###
  9  ##o#..............##C###
 10  ##R#.##G##o###,#########
 11  .....#o,o#####M#########
 12  ##o#####################
 13  ########################
```

| Symbole | Case | Les ouvriers y marchent |
| --- | --- | --- |
| `#` | Roche | Non |
| `.` | Route des ennemis | Oui |
| `,` | Galerie ou corniche réservée aux ouvriers | Oui |
| `o` | Emplacement de tourelle | Oui, même avec une tourelle dessus |
| `H` | Emplacement sur promontoire (portée ×1,3) | Oui |
| `R` | Éboulis | Seulement une fois déblayé |
| `G` | Gouffre | Seulement une fois le pont construit |
| `L` | Falaise | Seulement une fois l'échelle posée |
| `C` | Camp principal | Oui |
| `P` | Site d'avant-poste | Seulement une fois l'avant-poste monté |
| `M` | Mine d'or | Non (les mineurs se tiennent sur la case voisine) |
| `N` | Nid, dernière case de la route | Oui |

Coordonnées `(x, y)` :

- Camp principal `(20,9)`. Nid `(22,7)`. Mine `(14,11)`. Site d'avant-poste `(4,7)`.
- Galeries : `(9,7)`, `(7,11)`, `(20,8)`, `(14,10)`, `(5,5)`, `(5,6)`, `(5,7)`, `(5,8)`.
- Emplacements accessibles au départ : `(3,2)`, `(11,4)`, `(2,12)`, `(10,10)`, `(19,6)`.
- Emplacements à ouvrir : `(8,7)`, `(10,7)`, `(13,7)`, `(2,9)`, `(6,11)`, `(8,11)`.
- Promontoires à ouvrir : `(15,7)`, `(8,3)`.
- Éboulis : `(13,8)`, `(2,10)`. Gouffres : `(9,6)`, `(7,10)`. Falaises : `(14,7)`, `(7,3)`.

### Routes des ennemis

Deux routes de 27 cases chacune, qui se rejoignent en `(17,7)`. Les ennemis apparaissent hors carte en `x = −1`.

- **Passe Nord** : `(−1,3)` → `(6,3)` → `(6,5)` → `(17,5)` → `(17,7)` → `(22,7)`.
- **Passe Sud** : `(−1,11)` → `(4,11)` → `(4,9)` → `(17,9)` → `(17,7)` → `(22,7)`.

### Ce que chaque accès ouvre

| Chantier | Ouvre |
| --- | --- |
| Pont en `(9,6)` | Emplacements `(8,7)` et `(10,7)`, entre les deux routes |
| Déblayage en `(13,8)` | Emplacement `(13,7)`, et l'accès à la falaise `(14,7)` |
| Échelle en `(14,7)` (après le déblayage) | Promontoire `(15,7)`, la meilleure position |
| Échelle en `(7,3)` | Promontoire `(8,3)` |
| Déblayage en `(2,10)` | Emplacement `(2,9)` |
| Pont en `(7,10)` | Emplacements `(6,11)` et `(8,11)` |

La galerie `(5,5)`–`(5,8)` relie les deux routes à l'ouest pour les ouvriers, et dessert le site d'avant-poste.

## 3. Accessibilité

- Les ouvriers se déplacent de case en case, sur 4 voisins (pas de diagonale), par le plus court chemin (parcours en largeur).
- La **zone accessible** est l'ensemble des cases praticables reliées au camp principal. Elle est recalculée dès qu'un accès est ouvert.
- Un emplacement est constructible s'il est dans la zone accessible.
- Un éboulis, un gouffre, une falaise, la mine ou le site d'avant-poste peut recevoir un chantier s'il touche (4 voisins) une case de la zone accessible.
- Pour travailler, l'ouvrier se place sur une case praticable voisine du chantier, jamais sur le chantier.
- Pour un emplacement inaccessible, l'interface affiche la **chaîne d'accès** : les obstacles à ouvrir, dans l'ordre, depuis la zone accessible. Exemple pour `(15,7)` : déblayer `(13,8)`, puis échelle en `(14,7)`.

## 4. Ouvriers

Départ : 1 nain charpentier et 1 nain mineur, au camp principal. Équipe de 8 au maximum. Vitesse : 3,2 cases par seconde. Les ennemis ignorent les ouvriers, et inversement.

Les spécialités sont **strictes** : un ordre ne peut être exécuté que par un ouvrier de la bonne spécialité.

| Spécialité | Recrutement | Actions |
| --- | --- | --- |
| Nain mineur | 40 or | Déblayer un éboulis, ouvrir la mine, exploiter la mine |
| Nain charpentier | 50 or | Pont, échelle, avant-poste, baliste, mortier, démonter une baliste ou un mortier |
| Gnome ingénieur | 60 or | Bobine gnome, améliorer une tourelle, démonter une bobine |

Les recrues arrivent toujours au camp principal.

### États d'un ouvrier

| État | Sens | Disponible pour un nouveau chantier |
| --- | --- | --- |
| Au camp | Attend sur une case de camp | Oui |
| En route | Marche vers son chantier | Non |
| Au travail | Fait avancer son chantier | Non |
| Rentre au camp | Marche vers le camp le plus proche | Oui |
| Vers la mine | Mineur en chemin vers la mine | Oui, en dernier recours |
| À la mine | Mineur qui extrait de l'or | Oui, en dernier recours |

### Affectation, à chaque pas de simulation

1. Les chantiers sans ouvrier sont traités dans l'ordre de commande. Chacun prend l'ouvrier disponible de la bonne spécialité dont le trajet est le plus court. Un mineur à la mine ou en chemin vers elle n'est choisi que si aucun autre mineur n'est disponible.
2. Un mineur sans chantier descend à la mine si elle est ouverte et qu'il y reste une place.
3. Tout autre ouvrier sans chantier, s'il n'est pas déjà sur un camp, rentre immédiatement au camp le plus proche en longueur de trajet (camp principal ou avant-poste monté).

Un ouvrier interrompu entre deux cases rejoint d'abord le centre de sa case avant de repartir.

Un chantier sans ouvrier disponible reste en attente : il est déjà payé et sera pris dès qu'un ouvrier se libère.

## 5. Ordres et chantiers

Règles communes : l'or est débité à la commande ; un seul chantier par case à la fois ; le chantier avance seulement quand l'ouvrier est sur place.

| Ordre | Cible | Spécialité | Coût | Durée sur place |
| --- | --- | --- | --- | --- |
| Déblayer l'éboulis | Éboulis | Mineur | 30 or | 6 s |
| Jeter un pont | Gouffre | Charpentier | 60 or | 9 s |
| Poser une échelle | Falaise | Charpentier | 45 or | 6 s |
| Ouvrir la mine | Mine fermée | Mineur | 50 or | 8 s |
| Monter l'avant-poste | Site d'avant-poste | Charpentier | 70 or | 10 s |
| Construire une tourelle | Emplacement libre et accessible | Selon la tourelle | Selon la tourelle | Selon la tourelle |
| Améliorer | Tourelle de niveau 1 ou 2 | Ingénieur | Voir section 6 | 5 s |
| Démonter | Tourelle | Celle qui construit cette tourelle | 0 or | 3 s |

Un ordre est refusé, avec un message, dans cet ordre de vérification :

1. La partie est terminée.
2. Un chantier est déjà en cours sur la case.
3. La cible est inaccessible (« ouvre d'abord un accès »).
4. Aucun ouvrier de la spécialité dans l'équipe (« il faut un mineur dans l'équipe »).
5. Or insuffisant (« il manque N or »).

Le recrutement est refusé si l'équipe est complète ou si l'or manque.

## 6. Tourelles

| Tourelle | Spécialité | Coût | Chantier | Portée | Dégâts | Cadence | Effet |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Baliste | Charpentier | 50 or | 5 s | 2,8 | 15 | 1,1 tir/s | Une cible |
| Mortier | Charpentier | 90 or | 8 s | 3,3 | 26 | 0,42 tir/s | Zone de rayon 1,15 |
| Bobine gnome | Ingénieur | 80 or | 7 s | 2,3 | 5 | 1,25 tir/s | Touche 3 cibles et les ralentit |

- **Portée** : distance en ligne droite entre le centre de la case de la tourelle et l'ennemi. Sur un promontoire, la portée est multipliée par 1,3.
- **Ciblage** : parmi les ennemis à portée et entrés sur la carte (`x ≥ −0,4`), celui qui a parcouru le plus de chemin.
- **Rechargement** : `1 / cadence` secondes après chaque tir. Sans cible, la tourelle reste prête.
- **Baliste** : carreau guidé à 14 cases/s. Si la cible meurt avant l'impact, le carreau disparaît sans dégâts.
- **Mortier** : l'obus vise la position que la cible aura dans 0,7 s sur sa route (à sa vitesse du moment), vole 0,7 s, puis blesse tous les ennemis à 1,15 case ou moins du point d'impact.
- **Bobine gnome** : effet immédiat sur les 3 ennemis à portée les plus avancés. Chacun subit les dégâts et avance à vitesse ×0,5 pendant 1,6 s. Le ralentissement ne se cumule pas, sa durée est simplement relancée.

### Amélioration

Trois niveaux. Par niveau au-dessus du premier : dégâts +60 % et portée +8 % de la valeur de base.

- `dégâts = base × (1 + 0,6 × (niveau − 1))`
- `portée = base × (1,3 si promontoire) × (1 + 0,08 × (niveau − 1))`
- `coût = arrondi(coût de base × 0,7 × niveau actuel / 5) × 5`

| Tourelle | Niveau 1 → 2 | Niveau 2 → 3 |
| --- | --- | --- |
| Baliste | 35 or | 70 or |
| Mortier | 65 or | 125 or |
| Bobine gnome | 55 or | 110 or |

La tourelle continue de tirer pendant son amélioration.

### Démontage

- Rend `arrondi(0,75 × or investi)`, l'or investi étant le coût de construction plus toutes les améliorations payées.
- C'est un chantier : l'ouvrier de la spécialité qui construit cette tourelle doit venir, et l'or n'est rendu qu'à la fin des 3 secondes.
- La tourelle tire jusqu'à la fin du démontage. L'emplacement redevient libre.

## 7. Ennemis

La liste est provisoire et changera avec l'avancée du jeu. Elle doit être entièrement pilotée par les données.

| Ennemi | Points de vie | Vitesse | Or à la mort | Œufs détruits |
| --- | --- | --- | --- | --- |
| Bandit | 30 | 1,25 case/s | 6 | 1 |
| Aventurier | 21 | 2,3 cases/s | 6 | 1 |
| Soldat d'Arch-Dawn | 140 | 0,85 case/s | 18 | 2 |
| Capitaine d'Arch-Dawn (boss) | 1250 | 0,6 case/s | 120 | 5 |

- Points de vie à la vague `n` : `arrondi(base × (1 + 0,2 × (n − 1)))`.
- Un ennemi suit sa route sans jamais en dévier.
- Arrivé au nid, il détruit ses œufs et disparaît, sans donner d'or.

## 8. Vagues

Chaque groupe fait apparaître `nombre` ennemis sur sa route, le premier après `délai` secondes, puis un toutes les `intervalle` secondes.

| Vague | Route | Ennemi | Nombre | Intervalle | Délai |
| --- | --- | --- | --- | --- | --- |
| 1 | Nord | Bandit | 8 | 1,1 s | 0 |
| 2 | Sud | Bandit | 9 | 1,0 s | 0 |
| 3 | Nord | Bandit | 7 | 1,0 s | 0 |
| 3 | Sud | Bandit | 7 | 1,0 s | 2 s |
| 4 | Nord | Aventurier | 10 | 0,6 s | 0 |
| 4 | Sud | Bandit | 6 | 1,2 s | 3 s |
| 5 | Nord | Soldat | 2 | 3,0 s | 0 |
| 5 | Sud | Bandit | 10 | 0,9 s | 0 |
| 6 | Sud | Aventurier | 12 | 0,55 s | 0 |
| 6 | Nord | Bandit | 8 | 0,9 s | 0 |
| 6 | Nord | Soldat | 1 | (sans objet) | 8 s |
| 7 | Sud | Soldat | 3 | 2,5 s | 0 |
| 7 | Nord | Bandit | 12 | 0,8 s | 0 |
| 7 | Nord | Aventurier | 6 | 0,6 s | 10 s |
| 8 | Nord | Aventurier | 14 | 0,5 s | 0 |
| 8 | Sud | Aventurier | 14 | 0,5 s | 0 |
| 8 | Nord | Soldat | 2 | 3,0 s | 6 s |
| 8 | Sud | Soldat | 2 | 3,0 s | 6 s |
| 9 | Nord | Soldat | 3 | 2,4 s | 0 |
| 9 | Sud | Soldat | 3 | 2,4 s | 0 |
| 9 | Nord | Bandit | 10 | 0,7 s | 4 s |
| 9 | Sud | Bandit | 10 | 0,7 s | 4 s |
| 10 | Nord | Capitaine | 1 | (sans objet) | 0 |
| 10 | Sud | Soldat | 5 | 2,0 s | 0 |
| 10 | Nord | Bandit | 12 | 0,7 s | 3 s |
| 10 | Sud | Aventurier | 12 | 0,5 s | 8 s |

### Déroulé

- **Avant la vague 1** : pas de compte à rebours. Les ouvriers travaillent déjà. Le joueur lance la vague quand il veut.
- **Pendant une vague** : elle se termine quand tous ses ennemis sont apparus et qu'il n'en reste aucun en vie.
- **Fin de vague** `n` : prime de `25 + 6 × n` or, puis phase de préparation.
- **Préparation** : un compte à rebours lance automatiquement la vague suivante. Le joueur peut la lancer plus tôt et gagne 1 or par seconde entière restante.
- Les ordres sont possibles à tout moment : préparation, vague, pause.

### Menace et temps de préparation

Le temps de préparation dépend de la difficulté de la vague qui arrive.

- `menace(vague) = somme, sur ses groupes, de nombre × points de vie de base × (1 + 0,2 × (n − 1))`
- `ratio = menace / plus forte menace de la partie`
- `préparation = arrondi(20 + 20 × racine(ratio))` secondes
- `niveau affiché = max(1, plafond(5 × racine(ratio)))`, de 1 à 5

| Vague à venir | Menace | Niveau | Préparation | Prime de fin de vague |
| --- | --- | --- | --- | --- |
| 1 | 240 | 1 | lancée par le joueur | 31 or |
| 2 | 324 | 2 | 24 s | 37 or |
| 3 | 588 | 2 | 26 s | 43 or |
| 4 | 624 | 2 | 26 s | 49 or |
| 5 | 1044 | 2 | 28 s | 55 or |
| 6 | 1264 | 3 | 28 s | 61 or |
| 7 | 1993 | 3 | 31 s | 67 or |
| 8 | 2755 | 4 | 32 s | 73 or |
| 9 | 3744 | 4 | 34 s | 79 or |
| 10 | 7174 | 5 | 40 s | 85 or |

L'interface annonce la vague suivante : son niveau de menace et, par route, le nombre d'ennemis de chaque type.

## 9. Économie

- Départ : 170 or.
- Entrées : or des ennemis tués, prime de fin de vague, bonus de lancement anticipé, mine, démontage.
- Sorties : recrutement, chantiers, tourelles, améliorations.

### Mine d'or

- Fermée au départ. L'ordre « Ouvrir la mine » (mineur, 50 or, 8 s) la rend exploitable.
- 2 places. Chaque mineur sur place rapporte 2 or toutes les 6 secondes. La mine est sans fin.
- Les mineurs y vont d'eux-mêmes quand ils n'ont pas de chantier. Un déblayage commandé rappelle un mineur de la mine si aucun autre n'est libre.
- Les mineurs se tiennent sur la galerie `(14,10)`.

### Avant-poste

- Un seul site, imposé par la carte, en `(4,7)`. Une fois monté (charpentier, 70 or, 10 s), il compte comme un camp pour la règle de retour.
- Il ne sert pas au recrutement.

## 10. Fin de partie

- **Défaite** : 0 œuf. La partie s'arrête aussitôt.
- **Victoire** : la vague 10 est terminée avec au moins 1 œuf.
- L'écran de fin affiche : œufs restants, vague atteinte, ennemis abattus, or dépensé, or extrait de la mine, nombre de tourelles, et un bouton pour rejouer.

## 11. Commandes et interface

Vue de dessus, toute la carte visible. Tout se joue à la souris.

- **Clic sur une case** : la sélectionne. Un panneau affiche ce qu'elle est et les ordres possibles, avec pour chacun le coût, la spécialité requise, la durée du chantier et, s'il est refusé, la raison.
- **Emplacement sélectionné** : la portée de la tourelle (ou de la tourelle envisagée) est dessinée sur la carte.
- **Bandeau** : or, œufs, vague en cours sur 10, annonce de la vague suivante, compte à rebours, bouton « Lancer la vague » (qui affiche le bonus d'or en préparation).
- **Équipe** : liste des ouvriers avec spécialité et état, trois boutons de recrutement avec leur coût.
- **Temps** : pause, vitesse ×1, ×2, ×3, recommencer (avec confirmation).
- **Sur la carte** : barre de progression au-dessus de chaque chantier, signalée différemment tant qu'aucun ouvrier n'y est affecté ; barre de vie sur les ennemis blessés ; marque distincte pour les emplacements accessibles, inaccessibles et les promontoires ; indication des routes que prendra la vague suivante.
- **Messages brefs** : ordre accepté ou refusé, chantier terminé, accès ouvert (avec le nombre d'emplacements gagnés), œuf perdu, vague repoussée.

## 12. Prévu pour la suite (ne pas coder maintenant)

- **Chasseurs** : nouvelle spécialité, tours qui posent des pièges et tours de tireur d'élite.
- **Ingénieurs** : tours de soutien qui renforcent les tourelles autour d'elles.
- **Forgeron** : dès qu'il y en a un dans l'équipe, un clic sur le camp principal permet d'améliorer les performances des ouvriers. C'est sa seule utilité.
- **Grandes cartes** : plusieurs camps secondaires, toujours sur des sites imposés.
- **Ennemis** : la liste évoluera au fil du développement.

## 13. Cas de test de référence

Ces résultats viennent du prototype. Les tests du jalon 1 doivent les retrouver.

1. Au départ, exactement 5 emplacements sont accessibles : `(3,2)`, `(11,4)`, `(2,12)`, `(10,10)`, `(19,6)`.
2. Chaque route mesure 27 cases.
3. Chaîne d'accès de `(15,7)` : éboulis `(13,8)` puis falaise `(14,7)`. Chaîne de `(8,7)` : gouffre `(9,6)`.
4. Après le pont en `(9,6)`, 7 emplacements sont accessibles. Après le déblayage de `(13,8)` seul, 6.
5. La falaise `(14,7)` ne peut pas recevoir de chantier tant que `(13,8)` n'est pas déblayé.
6. Coûts d'amélioration : baliste 35 puis 70, mortier 65 puis 125, bobine 55 puis 110.
7. Une baliste de niveau 3 a coûté 155 or et son démontage rend 116 or, versés à la fin du chantier.
8. Temps de préparation avant les vagues 2 à 10 : 24, 26, 26, 28, 28, 31, 32, 34, 40 secondes. Niveaux de menace des vagues 1 à 10 : 1, 2, 2, 2, 2, 3, 3, 4, 4, 5.
9. Points de vie d'un bandit à la vague 6 : 60. D'un capitaine à la vague 10 : 3500.
10. Portée d'une baliste de niveau 1 sur promontoire : 3,64 cases. De niveau 3 hors promontoire : 3,248 cases.
11. Sans mineur dans l'équipe, « Déblayer » est refusé avec le motif de spécialité, même avec assez d'or.
12. Avec la mine ouverte et un seul mineur, un déblayage commandé le fait quitter la mine ; le chantier fini, il y retourne.
13. Un charpentier qui finit un chantier à l'ouest rentre à l'avant-poste s'il est monté, sinon au camp principal.
