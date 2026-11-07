# Mises à jour automatiques (launcher + Raspberry Pi)

Choix fait avec L'amie le 2026-11-07 : le Raspberry Pi garde la dernière version du jeu, et chaque joueur ouvre le jeu par un petit launcher qui le met à jour avant de le lancer. Les amis passent par Tailscale, comme pour le serveur de saves (`docs/SERVEUR_SAVES.md`). Aucun port n'est ouvert sur la box.

```
Ton PC                          Raspberry Pi (Tailscale)              PC d'un joueur
Packager le projet  ──publier──> builds (port 8788)  <──launcher──    Lancer Pokemon.bat
                                 + saves (port 8787)                    ├─ télécharge ce qui a changé
                                                                        └─ lance le jeu
```

- **À chaque ouverture**, le launcher demande au Pi la dernière version, compare avec les fichiers du PC et **ne télécharge que ceux qui ont changé**. Ensuite il lance le jeu.
- **Sans connexion** (Pi éteint, Tailscale coupé), il lance la version déjà installée.
- **Rien n'est cassé si on ferme la fenêtre** pendant un téléchargement : les anciens fichiers ne sont remplacés qu'une fois tout téléchargé et vérifié, et le téléchargement reprend là où il s'était arrêté.
- **Les saves ne sont jamais touchées** : elles ne font pas partie du build.
- **Multijoueur** : l'hôte refuse un joueur qui n'a pas la même version. Le message s'affiche dans le Pokédex > **En ligne**, avec la version du jeu installée.

| Fichier | Rôle |
| --- | --- |
| `Tools/Updater/pokemon_update_server.py` | Le serveur, sur le Pi (Python 3, rien à installer) |
| `Tools/Updater/pokemon-updates.service` | Le lance au démarrage du Pi |
| `Tools/Updater/publier_version.py` | Sur ton PC : envoie un nouveau build au Pi |
| `Tools/Updater/creer_decoupage_pak.py` | Dans l'éditeur, une seule fois : découpe le jeu en plusieurs .pak |
| `Tools/Updater/Launcher/` | Le launcher à donner à chaque joueur (`Lancer Pokemon.bat` + `launcher.ps1`) |
| `Source/PokemonNewAge/Core/GameVersionSubsystem.*` | Côté jeu : version installée, refus d'un monde d'une autre version |

## Pourquoi découper le jeu en plusieurs .pak

Par défaut, Unreal met tout le contenu dans un seul gros fichier. Changer une texture le change en entier, et il faudrait alors retélécharger plusieurs Go. Avec le découpage, chaque grand dossier de `Content` a son propre fichier (`pakchunk1`, `pakchunk2`...). Une modification dans `Content/Pokemon` ne fait retélécharger que le .pak de ce dossier.

Ce qui n'est rangé dans aucun dossier étiqueté (le moteur, les fichiers à la racine de `Content`) reste dans `pakchunk0`.

## 1. Le Pi : un disque pour les builds (une seule fois)

Le jeu pèse plusieurs Go, et le Pi garde les 3 dernières versions (les fichiers communs à plusieurs versions ne sont gardés qu'une fois). Il faut donc un disque ou une clé USB de **32 Go minimum**. La microSD du Pi est trop petite et s'userait vite.

1. Brancher le disque sur le Pi, puis s'y connecter : `ssh marie@pokeserveur.local`.
2. `lsblk -f` affiche les disques. Repérer le tien (souvent `sda`, avec sa partition `sda1`).
3. **Le formater en ext4 efface tout ce qu'il contient** : `sudo mkfs.ext4 -L jeu /dev/sda1`. Si c'est la clé de la copie de secours des saves, ne pas la formater : garder la même clé et utiliser `/mnt/secours` à la place de `/mnt/jeu` dans tout ce guide (et dans `pokemon-updates.service`).
4. Le monter au démarrage :
   ```
   sudo mkdir -p /mnt/jeu
   echo 'LABEL=jeu /mnt/jeu ext4 defaults,nofail 0 2' | sudo tee -a /etc/fstab
   sudo mount -a
   sudo mkdir -p /mnt/jeu/mises-a-jour && sudo chown marie:marie /mnt/jeu/mises-a-jour
   df -h /mnt/jeu        # doit afficher la taille du disque
   ```

## 2. Le Pi : installer le serveur (une seule fois)

Sur ton PC, dans un terminal ouvert dans le dossier du projet :

```
ssh marie@pokeserveur.local "mkdir -p ~/serveur-mises-a-jour"
scp Tools/Updater/pokemon_update_server.py Tools/Updater/pokemon-updates.service marie@pokeserveur.local:~/serveur-mises-a-jour/
```

Puis sur le Pi :

```
cd ~/serveur-mises-a-jour
python3 pokemon_update_server.py cle-publication     # note la clé : elle sert à publier, garde-la pour toi
nano config.json
```

Dans `config.json`, mettre `"data_dir": "/mnt/jeu/mises-a-jour"`. Pour n'écouter que sur Tailscale, remplacer aussi `"host": "0.0.0.0"` par l'adresse `100.x.x.x` du Pi (`tailscale ip -4`). Enregistrer avec Ctrl+O puis Entrée, quitter avec Ctrl+X.

Le lancer au démarrage :

```
sudo cp ~/serveur-mises-a-jour/pokemon-updates.service /etc/systemd/system/
sudo systemctl enable --now pokemon-updates
systemctl status pokemon-updates           # doit afficher « active (running) »
curl http://<100.x.x.x>:8788/health        # doit répondre {"ok": true, ..., "version": null}
```

Si ton nom d'utilisateur n'est pas `marie`, le remplacer dans `pokemon-updates.service` avant de le copier. Les messages du serveur se lisent avec `journalctl -u pokemon-updates -f`.

## 3. Ton PC : préparer le projet (une seule fois)

1. Récupérer `main`, puis recompiler (**PokemonNewAgeEditor Win64 Development Build**, Unreal fermé).
2. Ouvrir le projet, puis **Outils > Exécuter un script Python...** (*Tools > Execute Python Script*) > `Tools\Updater\creer_decoupage_pak.py`. Le Journal de sortie (*Output Log*) liste les dossiers et leur numéro de .pak. Ensuite, **Fichier > Tout enregistrer**.
   À relancer quand tu crées un nouveau grand dossier dans `Content` : les numéros déjà donnés ne changent jamais.
3. Vérifier dans **Modifier > Paramètres du projet > Packaging** que **Générer des segments** (*Generate Chunks*) est coché. C'est déjà réglé dans `Config/DefaultGame.ini`. Dans **Paramètres du projet > Gestionnaire de ressources** (*Asset Manager*), **Types de ressources primaires à analyser** doit contenir `PrimaryAssetLabel` (c'est le réglage par défaut d'Unreal).
4. Régler la publication : lancer une fois `python Tools\Updater\publier_version.py D:\Builds\Windows`. Le script crée `Tools\Updater\publier_config.json` et s'arrête. Ouvrir ce fichier et y mettre :
   ```
   {"server": "http://100.x.x.x:8788", "publish_key": "<la clé de publication du Pi>"}
   ```
   Ce fichier ne va jamais dans Git.

## 4. Publier une nouvelle version (à chaque fois)

1. Dans Unreal : **Plateformes > Windows > Packager le projet** (*Platforms > Windows > Package Project*), en configuration **Shipping**. Choisir toujours le même dossier, par exemple `D:\Builds`. Unreal crée `D:\Builds\Windows\` avec `PokemonNewAge.exe` dedans.
2. Vérifier que les .pak sont bien découpés : `D:\Builds\Windows\PokemonNewAge\Content\Paks\` doit contenir plusieurs `pakchunk...`.
3. Dans un terminal ouvert dans le dossier du projet :
   ```
   python Tools\Updater\publier_version.py D:\Builds\Windows --note "Arène d'Argenta"
   ```
   Le script affiche combien de fichiers changent et combien de Go partent. La première fois, tout le jeu part : comptez environ 2 minutes par Go, car le réseau du Pi 3B plafonne vers 10 Mo/s. Ensuite, seuls les .pak modifiés partent.
4. C'est tout : chaque launcher prendra la nouvelle version à sa prochaine ouverture.

Unreal ne refait pas toujours un .pak strictement identique même quand rien n'a changé dans son dossier. Si une publication envoie beaucoup plus que prévu, ce n'est pas grave, c'est juste plus long.

## 5. Installer le launcher chez chaque joueur (une seule fois)

1. Tailscale doit être installé et connecté, avec le Pi partagé (déjà fait si le serveur de saves marche, voir `docs/SERVEUR_SAVES.md`).
2. Créer un dossier, par exemple `D:\Jeux\Pokemon`, et y copier les deux fichiers de `Tools\Updater\Launcher\` : `Lancer Pokemon.bat` et `launcher.ps1`.
3. Double-cliquer sur **Lancer Pokemon.bat**. La première fois, le launcher crée `launcher_config.json` et s'arrête. Ouvrir ce fichier avec le Bloc-notes et remplacer `100.x.x.x` par l'adresse Tailscale du Pi.
4. Relancer **Lancer Pokemon.bat** : le jeu se télécharge dans `D:\Jeux\Pokemon\Jeu\`, puis se lance. Si Windows affiche « Windows a protégé votre ordinateur », cliquer sur **Informations complémentaires > Exécuter quand même** (le jeu n'est pas signé, c'est normal pour un jeu privé).
5. Pour un raccourci sur le Bureau : clic droit sur **Lancer Pokemon.bat** > **Afficher d'autres options** > **Envoyer vers > Bureau (créer un raccourci)**.

Ensuite, toujours ouvrir le jeu par ce raccourci, et plus directement par `PokemonNewAge.exe`, sinon il ne se met pas à jour. Les options passées au .bat vont au jeu (par exemple `Lancer Pokemon.bat -windowed`).

## Revenir à une version précédente

Si une version est cassée, sur le Pi :

```
cd ~/serveur-mises-a-jour
python3 pokemon_update_server.py versions                 # les 3 versions gardées
python3 pokemon_update_server.py utiliser 2026.11.07-143005
```

Les launchers reprennent cette version à leur prochaine ouverture.

## À savoir

- **En multijoueur**, l'hôte et les amis doivent avoir la même version. Si quelqu'un est refusé, il ferme le jeu et le relance par le launcher. Un jeu lancé depuis l'éditeur (version « dev ») est toujours accepté, pour les tests.
- La version installée s'affiche dans le Pokédex > **En ligne**, ligne **Version du jeu**.
- **Le launcher ne se met pas à jour tout seul.** S'il change un jour, il faudra recopier `launcher.ps1` chez chaque joueur.
- Lire les fichiers du jeu ne demande pas de clé : seuls ceux à qui tu partages le Pi sur Tailscale peuvent les télécharger. Publier demande la clé de publication.
- `config.json` et `data/` (sur le Pi), `publier_config.json` et `.cache_publication.json` (sur ton PC), `launcher_config.json` et `Jeu\` (chez les joueurs) ne vont jamais dans Git (`Tools/Updater/.gitignore`).
