# Caméra troisième personne réglable (SCRUM-22)

Guide pas à pas, sous Windows. Compte 10 minutes.

## Ce qui change

- **Molette de la souris** : rapprocher (vers l'avant) ou éloigner (vers l'arrière) la caméra, en douceur, entre 2 m et 8 m.
- La caméra **suit le personnage avec un léger retard**, moins rigide qu'avant.
- La caméra ne peut plus **passer sous le sol** ni se mettre **à la verticale** au-dessus du joueur (angle limité entre -60° et +30°).

Côté C++ :
- `APokemonNewAgeCharacter` : action `ZoomAction`, réglages `MinCameraDistance`, `MaxCameraDistance`, `ZoomStep`, `ZoomInterpSpeed` (catégorie **Camera** du Blueprint).
- `APokemonNewAgePlayerController` : `CameraPitchMin` / `CameraPitchMax` (catégorie **Camera** de `BP_ThirdPersonPlayerController`).

Tout se règle dans les Blueprints sans recompiler.

## 1. Récupérer le code et compiler

1. **Fermer Unreal.**
2. Dans le terminal de VS Code :
   ```
   git checkout claude/project-thread-5j8wl2
   git pull
   ```
3. **Ctrl+Maj+B** puis **PokemonNewAgeEditor Win64 Development Build** (avec **Editor** dans le nom). Attendre « Succeeded » avant d'ouvrir Unreal.
4. Ouvrir `PokemonNewAge.uproject`.

## 2. Créer l'action « zoom »

1. Content Browser (Ctrl+Espace ou « Tiroir à contenu ») : `Content/Input/Actions`.
2. Clic droit > **Input > Input Action**, nommer `IA_Zoom`.
3. L'ouvrir et régler **Value Type** sur **Axis1D (float)**. Enregistrer.

## 3. Lui donner la molette

1. Ouvrir `Content/Input/IMC_Default`.
2. Déplier **Default Key Mappings**, cliquer sur le **(+)** de **Mappings**, déplier **Mappings** et choisir `IA_Zoom` sur la ligne vide tout en bas.
3. Déplier la ligne, cliquer sur la case de touche et choisir **Mouse Wheel Axis** (en français « Axe de la molette de la souris » ; taper `wheel` ou `molette` pour filtrer, pas « Molette vers le haut / bas »). On ne peut pas « appuyer » sur la molette pour la choisir, il faut passer par la liste.
4. Enregistrer.

## 4. Brancher l'action sur le personnage

1. Ouvrir `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` > **Class Defaults**.
2. Chercher `zoom` dans le panneau Détails : régler **Zoom Action** sur `IA_Zoom`.
3. **Compiler** puis **Enregistrer**.

## 5. Tester

Ouvrir `Lvl_ThirdPerson` et lancer **Play** :
- Molette vers l'avant : la caméra se rapproche. Vers l'arrière : elle s'éloigne. Elle s'arrête aux limites.
- Bouger la souris de haut en bas : la caméra ne passe plus sous le sol.
- Courir (Maj) : la caméra suit avec un petit temps de retard.

Si ça ne va pas, les réglages sont dans **Class Defaults**, catégorie **Camera** (`BP_ThirdPersonCharacter` pour la distance, `BP_ThirdPersonPlayerController` pour les angles).

## 6. Enregistrer le travail dans Git

```
git add Content/Input Content/ThirdPerson/Blueprints
git commit -m "SCRUM-22 Brancher le zoom de la caméra dans l'éditeur"
git push
```
