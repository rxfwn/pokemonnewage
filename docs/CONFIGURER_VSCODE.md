# Configurer VS Code pour Unreal (SCRUM-18)

Guide pas à pas, sous Windows, une fois le projet créé (voir [CREER_PROJET_UE5.md](CREER_PROJET_UE5.md)). Compte 15 minutes.

Rappel : on écrit le code dans **VS Code**, mais c'est le compilateur de **Visual Studio** qui compile. Les deux doivent être installés.

## 1. Installer les extensions

1. Ouvrir VS Code, puis **Fichier > Ouvrir le dossier** et choisir `D:\Dev\pokemonnewage`.
2. VS Code propose d'installer les extensions recommandées du dépôt (fichier `.vscode/extensions.json`) : cliquer sur **Installer**. Sinon, onglet Extensions (Ctrl+Maj+X), taper `@recommended` et les installer une par une :
   - **C/C++** (Microsoft) : coloration, autocomplétion et débogage du C++
   - **C#** (Microsoft) : pour les fichiers de build `*.Build.cs` et `*.Target.cs`
   - **Claude Code** (Anthropic, badge vérifié)

## 2. Dire à Unreal d'utiliser VS Code

1. Ouvrir le projet : double-clic sur `D:\Dev\pokemonnewage\PokemonNewAge.uproject`.
2. **Édition > Préférences de l'éditeur > Général > Code source > Éditeur de code source** : choisir **Visual Studio Code**.
3. Redémarrer Unreal si demandé.
4. **Outils > Actualiser le projet Visual Studio Code**.

Unreal crée alors `PokemonNewAge.code-workspace` et des fichiers dans `.vscode/` (`c_cpp_properties.json`, `tasks.json`, `launch.json`…). Ils dépendent de ta machine : Git les ignore, chacun les régénère chez soi.

## 3. Ouvrir le workspace

1. Fermer VS Code.
2. Double-cliquer sur `D:\Dev\pokemonnewage\PokemonNewAge.code-workspace`.
3. Le workspace contient deux dossiers : **PokemonNewAge** (notre jeu) et **UE5** (le code source du moteur, en lecture). On ne modifie que PokemonNewAge.
4. Attendre que l'extension C/C++ finisse d'indexer (barre d'état en bas). La première fois, ça peut prendre 10 à 20 minutes à cause du moteur.

## 4. Compiler depuis VS Code

1. **Fermer Unreal** (sinon Live Coding bloque la compilation).
2. **Ctrl+Maj+B**, puis choisir la tâche **PokemonNewAgeEditor Win64 Development Build**.
3. Le terminal doit finir par `Result: Succeeded`.

## 5. Lancer et déboguer

1. Onglet **Exécuter et déboguer** (Ctrl+Maj+D).
2. Choisir **Launch PokemonNewAgeEditor (Development)** puis **F5**.
3. Unreal s'ouvre, relié au débogueur : un point d'arrêt (clic à gauche d'un numéro de ligne, par exemple dans `PokemonNewAgeCharacter.cpp`) arrêtera le jeu quand la ligne est exécutée en Play.

## Au quotidien

- **Modifier du C++ avec Unreal ouvert** : enregistrer dans VS Code, puis dans Unreal **Ctrl+Alt+F11** (Live Coding) pour recompiler sans fermer l'éditeur.
- **Ajouter ou supprimer un fichier `.cpp`/`.h`** : refaire **Outils > Actualiser le projet Visual Studio Code**, sinon l'autocomplétion ne le voit pas.
- **Ajouter une classe C++** : de préférence depuis Unreal (**Outils > Nouvelle classe C++**), qui crée les fichiers au bon endroit.

## En cas de souci

- **Soulignements rouges partout dans le code** : l'indexation n'est pas finie, ou le projet VS Code n'a pas été actualisé (étape 2.4).
- **Aucune tâche de build proposée** : tu as ouvert le dossier au lieu du fichier `.code-workspace` (étape 3).
- **« Unable to build while Live Coding is active »** : fermer Unreal, ou utiliser Ctrl+Alt+F11 dans Unreal.
- Pour tout le reste : copier le message d'erreur à Claude.
