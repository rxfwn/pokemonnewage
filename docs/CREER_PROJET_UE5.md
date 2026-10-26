# Créer le projet Unreal Engine 5 (SCRUM-17)

Guide pas à pas, sous Windows. Compte environ 1 h 30, surtout pour les téléchargements (UE5 et Visual Studio pèsent plusieurs dizaines de Go : prévoir **150 Go libres** sur le disque).

## 1. Installer les outils

1. **Git** : https://git-scm.com/download/win (options par défaut). Git LFS est inclus.
2. **Epic Games Launcher** : https://store.epicgames.com/download, puis se connecter avec un compte Epic.
3. Dans le Launcher : onglet **Unreal Engine** > **Bibliothèque** > bouton **+** à côté de « Versions du moteur » > choisir la **dernière version 5.x** > **Installer**.
4. **Visual Studio 2022 Community** : https://visualstudio.microsoft.com/fr/vs/community/. Dans l'installateur, cocher la charge de travail **Développement de jeux en C++**, et dans le panneau de droite cocher **Unreal Engine installer** et le dernier **Windows SDK**. VS sert seulement à compiler : on code dans VS Code.
5. **VS Code** (si pas déjà installé) avec l'extension **C/C++** de Microsoft.

## 2. Cloner le dépôt

Dans un terminal (PowerShell) :

```powershell
git lfs install
mkdir D:\Dev
cd D:\Dev
git clone https://github.com/rxfwn/pokemonnewage.git
```

Mettre le projet sur le disque qui a le plus de place (ici `D:`). Éviter un dossier dans OneDrive ou avec des accents dans le chemin.

## 3. Créer le projet

Unreal crée toujours un dossier au nom du projet, donc on le crée à côté puis on déplace les fichiers dans le dépôt.

1. Lancer Unreal Engine depuis le Launcher.
2. Catégorie **Jeux** > modèle **Third Person**.
3. À droite : **C++** (pas Blueprint), qualité **Maximale**, **Contenu de démarrage** coché, Raytracing décoché pour l'instant.
4. Emplacement : `D:\Dev\tmp`. Nom du projet : `PokemonNewAge`. Cliquer sur **Créer**.
5. Attendre la compilation et l'ouverture de l'éditeur, puis **fermer Unreal**.
6. Déplacer depuis `D:\Dev\tmp\PokemonNewAge\` vers `D:\Dev\pokemonnewage\` :
   - `PokemonNewAge.uproject`
   - les dossiers `Config`, `Content`, `Source`
   - Ne pas copier `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `.vs`, ni le `.sln`.
7. Supprimer `D:\Dev\tmp`.

## 4. Vérifier que le projet s'ouvre depuis le dépôt

1. Clic droit sur `D:\Dev\pokemonnewage\PokemonNewAge.uproject` > **Generate Visual Studio project files**.
2. Double-cliquer sur `PokemonNewAge.uproject`. Si Unreal propose de recompiler les modules, répondre **Oui**.
3. Le niveau de démo Third Person s'ouvre : cliquer sur **Play** pour vérifier qu'on peut marcher.
4. Pour que VS Code soit l'éditeur de code : **Édition > Préférences de l'éditeur > Code source > Accesseur de code source = Visual Studio Code**, puis **Outils > Actualiser le projet Visual Studio Code**.

## 5. Committer

Toujours dans `D:\Dev\pokemonnewage` :

```powershell
git checkout -b feature/ue5-project
git add .
git status
```

Vérifier dans `git status` qu'il n'y a **aucun** fichier de `Binaries/`, `Intermediate/`, `Saved/` ou `DerivedDataCache/`. Puis :

```powershell
git lfs ls-files     # doit lister les .uasset et .umap
git commit -m "SCRUM-17 Créer le projet UE5 (Third Person C++)"
git push -u origin feature/ue5-project
```

Ensuite, ouvrir la Pull Request sur GitHub, ou demander à Claude de le faire.

## En cas de souci

- **« Could not be compiled »** à l'ouverture : il manque la charge de travail C++ ou le Windows SDK dans Visual Studio Installer.
- **Push refusé à cause de la taille** : Git LFS n'était pas actif au moment du commit. Ne pas insister : envoyer le message à Claude, qui t'aidera à refaire le commit proprement.
- Pour tout le reste : copier le message d'erreur à Claude.
