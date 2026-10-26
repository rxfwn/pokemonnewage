# Marcher et courir (SCRUM-21)

Guide pas à pas, sous Windows. Compte 15 minutes.

## Ce qui change

- Par défaut le joueur **marche** (250 cm/s) au lieu de trottiner.
- En maintenant **Maj gauche** (ou en cliquant le stick gauche à la manette), il **court** (500 cm/s).
- Les animations existent déjà dans le mannequin du modèle Third Person : l'Animation Blueprint choisit repos, marche ou course selon la vitesse. Aucun nouvel asset d'animation n'est nécessaire pour l'instant (on les remplacera quand on aura le vrai modèle du joueur).

Côté C++ (`Source/PokemonNewAge/PokemonNewAgeCharacter.*`) : une action `SprintAction` et deux vitesses réglables `WalkSpeed` / `SprintSpeed`, modifiables dans le Blueprint sans recompiler.

Il reste à faire dans l'éditeur : créer l'action de course, lui donner une touche, et la brancher sur le personnage.

## 1. Récupérer le code et compiler

1. Fermer Unreal.
2. Dans un terminal ouvert sur `D:\Dev\pokemonnewage` :
   ```
   git fetch origin
   git checkout claude/project-thread-5j8wl2
   ```
3. Dans VS Code (workspace `PokemonNewAge.code-workspace`), **Terminal > Exécuter la tâche de build** (Ctrl+Maj+B) et choisir **PokemonNewAgeEditor Win64 Development Build**. Attendre « Succeeded ».
4. Ouvrir `PokemonNewAge.uproject`.

## 2. Créer l'action « courir »

1. Dans le Content Browser, aller dans `Content/Input/Actions`.
2. Clic droit > **Input > Input Action**. Nommer l'asset `IA_Sprint`.
3. Double-cliquer dessus : vérifier que **Value Type** vaut **Digital (bool)**. Enregistrer.

## 3. Lui donner une touche

1. Ouvrir `Content/Input/IMC_Default`.
2. À côté de **Mappings**, cliquer sur **+** et choisir `IA_Sprint` dans la nouvelle ligne.
3. Cliquer sur l'icône clavier puis appuyer sur **Maj gauche** (Left Shift).
4. Cliquer sur le **+** à droite de `IA_Sprint` pour ajouter une 2e touche : **Gamepad Left Thumbstick Button**.
5. Enregistrer.

## 4. Brancher l'action sur le personnage

1. Ouvrir `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`.
2. Cliquer sur **Class Defaults** (barre du haut).
3. Dans le panneau Details, catégorie **Input** : régler **Sprint Action** sur `IA_Sprint`.
4. Catégorie **Movement** : on y voit **Walk Speed** (250) et **Sprint Speed** (500). On pourra les ajuster ici plus tard.
5. **Compile** puis **Save**.

## 5. Tester

1. Ouvrir `Content/ThirdPerson/Lvl_ThirdPerson` et lancer **Play**.
2. Avancer sans rien d'autre : le personnage **marche**.
3. Maintenir **Maj** : il **court**. Relâcher : il revient à la marche.
4. Le saut (Espace) marche toujours.

Si le personnage ne court pas : vérifier l'étape 4 (Sprint Action vide) puis l'étape 3 (touche).

## 6. Enregistrer le travail dans Git

Les fichiers modifiés sont `IA_Sprint.uasset` (nouveau), `IMC_Default.uasset` et `BP_ThirdPersonCharacter.uasset`.

```
git add Content/Input Content/ThirdPerson/Blueprints
git commit -m "SCRUM-21 Brancher la touche de course dans l'éditeur"
git push
```

Ensuite, prévenir Claude dans le fil : la PR sera prête à fusionner.
