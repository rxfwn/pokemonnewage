# Entrer dans les bâtiments sans chargement (SCRUM-25) et menu en jeu

Le jeu est un monde ouvert : on entre dans une maison comme dans la vraie vie, en passant la porte. Pas d'écran noir, pas de changement de map. Les bâtiments ont leur vraie taille, l'intérieur est directement dedans.

## Ce qui est en place

- **Les portes s'ouvrent toutes seules** quand on approche, du côté opposé au joueur, et se referment 1,5 s après qu'on s'est éloigné.
- **La touche E** (bouton gauche de la manette) ouvre ou ferme une porte à portée. C'est la même touche qui servira plus tard à parler aux PNJ, lire les panneaux et ramasser les objets.
- **À l'intérieur, la caméra se rapproche** (2,5 m au plus) pour ne pas traverser les murs ni le plafond. Elle reprend sa distance en sortant.
- **Les bâtiments du blockout sont creux** : murs, porte au sud, sol, plafond, toit et quelques meubles de blockout (table, télé et escalier chez le joueur ; table des Poké Balls et bibliothèques au labo ; comptoir au Centre Pokémon et à la Boutique).
- **L'arène de Jadielle reste fermée à clé**, comme dans le jeu.

La touche E marche sans rien régler : le code la crée tout seul. Pour la changer, créer un Input Action `IA_Interact` et le choisir dans **Interact Action** du Blueprint `BP_ThirdPersonCharacter`.

## Pas à pas (10 minutes)

1. **Fermer Unreal.**
2. Dans le terminal de VS Code :
   ```
   git checkout claude/project-thread-5j8wl2
   git pull
   ```
3. **Ctrl+Maj+B** puis **PokemonNewAgeEditor Win64 Development Build**. Attendre « Succeeded ».
4. Ouvrir le projet et la map `L_Kanto`.
5. **Outils > Exécuter un script Python…** > `Tools\Blockout\blockout_kanto_v1.py`. Le script refait le blockout avec les bâtiments creux (tes objets rangés hors du dossier `Kanto_Blockout` ne sont pas touchés).
6. **Play** et tester :
   - marcher vers la porte de ta maison : elle s'ouvre, on entre, la caméra se rapproche ;
   - ressortir : la porte se referme derrière toi ;
   - devant une porte, appuyer sur **E** pour l'ouvrir ou la fermer ;
   - aller voir le labo, puis le Centre Pokémon et la Boutique de Jadielle ; l'arène de Jadielle ne s'ouvre pas.
7. Tester le **menu en jeu** : appuyer sur **Tab** (ou Start à la manette). Le jeu se met en pause et le menu s'affiche à droite. Les flèches ou la souris choisissent une entrée ; **Reprendre**, **Tab** ou **Retour arrière** ferment le menu. **Quitter** arrête la partie. Les autres entrées sont grisées « (bientôt) » pour l'instant. Dans l'éditeur, **Échap** arrête le Play : c'est pour ça que le menu est sur Tab.
8. Enregistrer la map dans Git :
   ```
   git add Content
   git commit -m "SCRUM-25 Bâtiments creux avec portes dans L_Kanto"
   git push
   ```

Si le journal affiche **« Classes Door / InteriorVolume introuvables »**, c'est que l'étape 3 n'a pas été faite (ou qu'Unreal était ouvert pendant la compilation) : les bâtiments sont posés, mais sans portes.

## Avec tes propres modèles 3D

Quand tu remplaces une maison du blockout par ton modèle :

1. Le modèle doit être creux, avec une ouverture de porte d'environ **1,4 m de large et 2,4 m de haut**.
2. **Placer une porte** : panneau **Placer des acteurs**, chercher **Door**, la glisser dans l'ouverture. Son axe X (flèche rouge) doit sortir du bâtiment. Dans Détails, régler **Door Width** et **Door Height** à la taille de l'ouverture.
3. **Ton propre modèle de porte** : clic droit sur la classe `Door` dans le Content Browser (dossier C++ Classes) > **Créer une classe Blueprint basée sur Door**, par exemple `BP_Door_House`, puis changer le mesh du composant **Panel**.
4. **Placer un Interior Volume** (même panneau, chercher **Interior Volume**) et l'étirer pour qu'il remplisse l'intérieur, du sol au plafond.
5. Ranger le tout dans ton dossier (par exemple `Kanto_Art`), pas dans `Kanto_Blockout`.
