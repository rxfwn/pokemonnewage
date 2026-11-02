# Serveur de saves (Raspberry Pi)

Le Raspberry Pi à la maison garde la save de chaque joueur, en plus de la copie sur son PC. Choix fait avec L'amie le 2026-11-02 (voir `docs/MULTIJOUEUR.md`).

- **Le jeu ne tourne pas sur le Pi.** Il ne fait que garder les saves et les cartes dresseur. Jouer à plusieurs passe toujours d'un PC à l'autre.
- **Au lancement**, le jeu récupère la save du serveur si elle est plus récente que celle du PC (partie faite sur un autre PC). Si c'est celle du PC qui est plus récente (partie hors ligne), elle part sur le serveur. Le lancement n'attend jamais plus de 4 secondes.
- **À chaque sauvegarde**, la save et la carte dresseur partent en arrière-plan. Si le serveur ne répond pas, on joue normalement, et le jeu réessaie toutes les minutes.
- **Sécurité minimale** : chaque joueur a une clé secrète et ne peut lire ou écrire que sa propre save. Le serveur n'est joignable que par Tailscale, et aucun port n'est ouvert sur la box. Le serveur garde aussi les 10 dernières versions de chaque save, et une copie part chaque nuit sur une clé USB.

| Fichier | Rôle |
| --- | --- |
| `Tools/SaveServer/pokemon_save_server.py` | Le serveur (Python 3, aucune bibliothèque à installer, marche aussi sur Windows) |
| `Tools/SaveServer/pokemon-saves.service` | Le lance au démarrage du Pi |
| `Tools/SaveServer/sauvegarde_secours.sh` | Copie de secours sur la clé USB, chaque nuit |
| `Source/PokemonNewAge/Core/SaveServerSubsystem.*` | Côté jeu : récupère et envoie les saves |

## Essayer sur ton PC d'abord (sans le Pi)

1. Installer Python depuis python.org, en cochant **Add python.exe to PATH**.
2. Dans l'Explorateur, ouvrir `Tools\SaveServer`, taper `cmd` dans la barre d'adresse, puis Entrée.
3. `python pokemon_save_server.py ajouter-joueur Marie`, puis noter la clé affichée.
4. `python pokemon_save_server.py` : le serveur tourne tant que la fenêtre est ouverte. Windows peut demander d'autoriser Python dans le pare-feu.
5. Dans le jeu, console (**²**) : `ServeurSaves http://127.0.0.1:8787 Marie <la clé>`. Puis sauvegarder (F5). Le Pokédex > **En ligne** doit afficher « Serveur de saves : à jour », et la fenêtre du serveur affiche `save de Marie : version 1`.

## Installer le Pi (environ 45 minutes la première fois)

### 1. Préparer la carte microSD (sur ton PC)

1. Ouvrir **Raspberry Pi Imager**, puis choisir :
   - **Modèle** : Raspberry Pi 3 ;
   - **Système d'exploitation** : Raspberry Pi OS (other) > **Raspberry Pi OS Lite (64-bit)**, sans bureau, plus léger ;
   - **Stockage** : la carte microSD.
2. À la question sur la personnalisation, choisir **Modifier les réglages** :
   - nom d'hôte : `pokeserveur` ;
   - nom d'utilisateur : `marie`, avec un mot de passe à retenir ;
   - Wi-Fi : seulement si le Pi ne sera pas branché en Ethernet ;
   - fuseau horaire : Europe/Paris, clavier `fr` ;
   - onglet **Services** : cocher **Activer SSH** (avec mot de passe).
3. **Écrire**, attendre la fin, puis mettre la carte dans le Pi.

### 2. Le démarrer et s'y connecter

1. Brancher le câble Ethernet sur la box, puis l'alimentation. Attendre 2 minutes : le voyant vert clignote, puis se calme.
2. Sur le PC, ouvrir **Terminal** (ou PowerShell) et taper `ssh marie@pokeserveur.local`. Répondre `yes` la première fois, puis donner le mot de passe.
3. Vérifier le Pi :
   - `vcgencmd get_throttled` doit répondre `throttled=0x0` (pas de manque de courant ni de surchauffe) ;
   - `free -h` affiche la mémoire, `df -h` le stockage.
4. Mettre à jour : `sudo apt update && sudo apt full-upgrade -y`, puis `sudo reboot` et se reconnecter.

### 3. Tailscale

1. Sur le Pi : `curl -fsSL https://tailscale.com/install.sh | sh`, puis `sudo tailscale up`. Ouvrir le lien affiché sur le PC et se connecter avec le compte Tailscale.
2. `tailscale ip -4` affiche l'adresse du Pi sur Tailscale (`100.x.x.x`). La noter.
3. Sur le site Tailscale, page **Machines**, menu **…** du Pi : **Disable key expiry**, pour que le Pi ne se déconnecte pas au bout de 6 mois.

### 4. Copier le serveur

Sur le PC, dans un autre terminal ouvert dans le dossier du projet :

```
ssh marie@pokeserveur.local "mkdir -p ~/serveur-saves"
scp Tools/SaveServer/pokemon_save_server.py Tools/SaveServer/sauvegarde_secours.sh marie@pokeserveur.local:~/serveur-saves/
```

Puis, sur le Pi :

```
cd ~/serveur-saves
python3 pokemon_save_server.py ajouter-joueur Marie
python3 pokemon_save_server.py ajouter-joueur <PrénomAmi>
```

Noter chaque clé : elle se donne en privé à chaque joueur. `python3 pokemon_save_server.py joueurs` les liste.

Pour n'écouter que sur Tailscale, ouvrir `nano config.json` et remplacer `"host": "0.0.0.0"` par l'adresse `100.x.x.x` du Pi. Enregistrer avec Ctrl+O puis Entrée, quitter avec Ctrl+X.

### 5. Le lancer tout seul au démarrage

```
scp Tools/SaveServer/pokemon-saves.service marie@pokeserveur.local:~/serveur-saves/   # sur le PC
sudo cp ~/serveur-saves/pokemon-saves.service /etc/systemd/system/                    # sur le Pi
sudo systemctl enable --now pokemon-saves
systemctl status pokemon-saves        # doit afficher « active (running) »
curl http://<100.x.x.x>:8787/health   # doit répondre {"ok": true, ...}
```

Si ton nom d'utilisateur n'est pas `marie`, remplacer `marie` dans `pokemon-saves.service` avant de le copier. Les messages du serveur se lisent avec `journalctl -u pokemon-saves -f`.

### 6. Copie de secours sur la clé USB

1. Brancher la clé, puis `lsblk -f` pour repérer sa partition (par exemple `sda1`) et noter son `UUID`.
2. `sudo mkdir -p /mnt/secours`, puis `sudo nano /etc/fstab` et ajouter la ligne suivante (pour une clé en FAT32/exFAT) :
   `UUID=<l'UUID> /mnt/secours auto defaults,nofail,uid=1000,gid=1000 0 0`
3. `sudo mount -a`, puis vérifier avec `ls /mnt/secours`.
4. `crontab -e`, choisir `nano`, et ajouter à la fin :
   `0 4 * * * /home/marie/serveur-saves/sauvegarde_secours.sh`
   Une archive part chaque nuit à 4 h, et elle est gardée 30 jours.

## Brancher le jeu (chaque joueur, une seule fois)

1. Installer Tailscale sur son PC. Si c'est un ami : dans la page **Machines** de ton compte Tailscale, menu **…** du Pi > **Share** (Partager), et lui envoyer l'invitation. Il ne verra que le Pi. Pour qu'il rejoigne ton monde, partage aussi ton PC de la même façon.
2. Dans le jeu, console (**²**) : `ServeurSaves http://100.x.x.x:8787 <son nom> <sa clé>`.
3. `ServeurSaves` sans rien affiche l'état. `ServeurSaves aucun` retire le serveur (la save reste seulement sur le PC).
4. Le Pokédex > **En ligne** affiche « Serveur de saves : à jour », et les cartes des amis viennent alors du serveur (plus besoin du dossier partagé).

## Revenir à une ancienne save

Les 10 versions précédentes sont dans `~/serveur-saves/data/saves/<joueur>/historique/` (`v000012.sav`...). Pour en remettre une :
1. Arrêter le serveur : `sudo systemctl stop pokemon-saves`.
2. La copier par-dessus `current.sav`.
3. Dans `meta.json`, mettre `saved_at` à l'heure actuelle (en UTC).
4. Relancer le serveur : `sudo systemctl start pokemon-saves`.

Le jeu la récupère au prochain lancement.

## À savoir

- **Nouvelle partie** : l'ancienne save reste sur le serveur jusqu'à la première sauvegarde de la nouvelle partie. Si tu quittes avant de sauvegarder, l'ancienne revient au lancement suivant (et c'est voulu, ça évite de la perdre par erreur).
- **Deux PC avec la même save en même temps** : c'est la dernière sauvegarde qui gagne. L'autre version reste dans l'historique.
- Les fenêtres de test en plus dans l'éditeur (`Partie_PIE1`...) n'utilisent jamais le serveur.
- `config.json` (les clés) et `data/` ne vont jamais dans Git (`Tools/SaveServer/.gitignore`).
