# Monde ouvert de la V1 : de Bourg Palette à Argenta (SCRUM-62)

Le jeu est un **monde ouvert** : une seule grande map, `L_Kanto`, au lieu d'une map par lieu. Unreal ne charge que les morceaux proches du joueur grâce au **World Partition**, l'outil prévu pour les grands mondes.

On travaille du grand vers le petit :

1. **Le terrain de toute la V1** (ce guide) : le relief, les chemins, les villes en blocs simples, les arbres et la lumière. On le traverse à pied pour régler les distances.
2. **Le détail, zone par zone** ensuite : Bourg Palette (SCRUM-23), Route 1 (SCRUM-24), etc., d'après les fiches du tableau Miro.

## Ce qui est réaliste dès maintenant

- **Les tailles** : un personnage fait 1,80 m, une maison 9 × 11 m et 7 m de haut, une arène 26 × 30 m, les arbres 12 à 20 m. Les bâtiments ont leur vraie taille, ce qui laisse la place à des intérieurs sans chargement plus tard.
- **La forme de Kanto** : le terrain suit la carte officielle de *Let's Go* (terre et mer extraites de l'image). Tout Kanto est déjà là, entouré de mer : environ 2,7 × 1,9 km sur un terrain de 3 km de côté. Seuls les lieux de la V1 (de Bourg Palette à Argenta) sont jouables pour l'instant, le reste est du relief à détailler plus tard.
- **Le relief** : des collines boisées autour des routes, le Mont Sélénite, la Route Victoire, des montagnes derrière Argenta (jusqu'à environ 185 m). Les routes serpentent et les bords des villes sont arrondis, comme de vraies vallées. Elles sont bordées de talus trop raides pour être escaladés : pas de murs invisibles au milieu des champs.
- **La lumière** : ciel physique, soleil d'après-midi, nuages volumétriques et brume légère.
- **Le sol** : herbe sur le plat, roche sur les pentes raides, sable au bord de la mer.

Les couleurs restent des couleurs de blockout : les vrais modèles et textures viendront après.

## Le plan

Vu de dessus, nord en haut. Environ 3 km de côté, la V1 est la bande verte à l'ouest.

![Aperçu du relief](../Tools/Blockout/kanto_v1_preview.png)

| Lieu | Taille | Distance depuis Bourg Palette |
|---|---|---|
| Bourg Palette | 110 × 120 m | départ |
| Route 1 | 230 m de long | |
| Jadielle | 140 × 160 m | environ 300 m |
| Route 22 (vers l'ouest) | 240 m de long, finit au Poste de la Ligue (fermé) | |
| Route 2 (sud) puis Forêt de Jade (180 × 200 m) puis Route 2 (nord) | | |
| Argenta | 120 × 160 m | environ 850 m |

Bourg Palette → Argenta : environ **6 minutes en marchant, 3 minutes en courant**. À ajuster après le test.

Le plan est écrit dans `Tools/Blockout/kanto_v1_layout.py` (lieux repérés en pixels sur la carte de *Let's Go*, bâtiments, chemins, hautes herbes). La forme terre / mer est dans `Tools/Blockout/kanto_terres.bin`. Pour changer quelque chose, on modifie ce fichier, puis on relance le script.

## Pas à pas (30 minutes)

### 1. Récupérer le travail

1. **Fermer Unreal.**
2. Dans le terminal de VS Code :
   ```
   git checkout claude/project-thread-5j8wl2
   git pull
   ```
3. **Ctrl+Maj+B** puis **PokemonNewAgeEditor Win64 Development Build**. Attendre « Succeeded ».
4. Ouvrir `PokemonNewAge.uproject`.

### 2. Créer la map du monde ouvert

1. **Fichier > Nouveau niveau**, choisir **Empty Open World** (« Monde ouvert vide »), puis **Créer**. Ce modèle active le World Partition.
2. **Fichier > Enregistrer le niveau sous** : créer le dossier `Kanto` dans `Content/World`, nommer la map **`L_Kanto`**.

### 3. Importer le relief

1. Passer en **mode Paysage** (Maj+2, ou le menu des modes en haut à gauche > **Paysage**).
2. Onglet **Gérer** > **Nouveau** > cocher **Importer depuis un fichier**.
3. **Fichier de heightmap** : `D:\Dev\pokemonnewage\Tools\Blockout\kanto_v1_heightmap.png`.
4. Régler :
   - **Emplacement** : 0, 0, 0 ;
   - **Échelle** : **150, 150, 100**. C'est important : 150 fait 1,5 m entre deux points, donc un terrain de 3 km (2017 points de côté) ;
   - laisser la taille des sections proposée par Unreal (elle se règle d'après le fichier).
5. Cliquer sur **Importer**. Le relief apparaît (gris quadrillé pour l'instant, le script lui donne une couleur). L'import peut prendre une minute.
6. Revenir en **mode Sélection** (Maj+1) et **enregistrer** (Ctrl+S).

### 4. Lancer le script

1. **Outils > Exécuter un script Python…**
2. Choisir `D:\Dev\pokemonnewage\Tools\Blockout\blockout_kanto_v1.py`.
3. Patienter **2 à 3 minutes** : une barre de progression s'affiche. Le script pose environ 2 400 objets (dont un millier d'arbres), rangés dans le dossier **Kanto_Blockout** de l'Outliner, puis enregistre tout.

Le script efface et repose tout à chaque lancement : on peut le relancer après une modification du plan.

### 5. Tester

1. **Play**. On démarre à Bourg Palette, entre les deux maisons, face au nord.
2. Vérifier :
   - aller jusqu'à Argenta à pied, puis en courant : est-ce trop long, trop court ?
   - les maisons et l'arène ont une taille crédible à côté du personnage ;
   - on ne peut pas sortir des routes (talus trop raides) ni tomber à l'eau ;
   - on traverse bien les postes de la Forêt de Jade ; le Poste de la Ligue, au bout de la Route 22, est fermé ;
   - la lumière et le ciel rendent bien.
3. Noter ce qui ne va pas et me l'envoyer (une capture aide beaucoup).

### 6. Enregistrer dans Git

Avec le World Partition, chaque objet est un petit fichier dans `Content/__ExternalActors__` : c'est normal qu'il y en ait beaucoup.

```
git status
git add Content
git commit -m "SCRUM-62 Ajouter la map L_Kanto avec le relief et le blockout de la V1"
git push
```

`git add Content` prend aussi `IMC_Default` (le clavier en ZQSD), qui attendait d'être enregistré.

L'ancienne map `L_PalletTown` ne sert plus : on peut supprimer le dossier `Content/World/PalletTown` dans Unreal (clic droit > Supprimer) avant le commit.

## Si ça ne marche pas

- **Pas de modèle « Empty Open World »** : créer un niveau **Basic**, l'enregistrer en `L_Kanto`, puis **Outils > Convertir le niveau** et cocher le World Partition.
- **« Pas de Landscape »** dans le journal : l'étape 3 n'a pas été faite, ou le Landscape est dans une autre map.
- **« Échelle du Landscape inattendue »** : l'échelle de l'étape 3 n'était pas 150, 150, 100. Supprimer le Landscape et le réimporter.
- **Le sol reste gris quadrillé** : sélectionner le Landscape, panneau Détails, **Landscape Material** : choisir `M_Kanto_Sol` (dans `Content/World/Kanto/Blockout`).
- **Un nom de lieu s'affiche à l'envers** : chaque panneau a deux faces, une seule est à l'endroit selon le côté d'où on regarde.
- **Le script ne fait rien** : **Fenêtre > Journal de sortie**, chercher les lignes `LogPython`, et m'envoyer le message.

## Refaire le relief

Le relief vient de `Tools/Blockout/make_heightmap.py`, qui lit le même plan. Après une modification du plan, le relancer (`python Tools/Blockout/make_heightmap.py`, quelques minutes), puis réimporter le PNG dans le Landscape (mode Paysage > Gérer > Importer) et relancer le script de blockout.
