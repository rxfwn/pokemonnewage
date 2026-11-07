#!/bin/sh
# Copie de secours des saves sur la clé USB, lancée chaque nuit (docs/SERVEUR_SAVES.md, étape 6).
# Garde une archive par jour pendant 30 jours.
set -e
SOURCE="$HOME/serveur-saves"
DEST="/mnt/secours/saves-pokemon"
if ! mountpoint -q /mnt/secours; then
    echo "Clé USB absente : pas de copie de secours" >&2
    exit 1
fi
mkdir -p "$DEST"
tar -czf "$DEST/saves-$(date +%Y-%m-%d).tar.gz" -C "$SOURCE" data config.json
find "$DEST" -name 'saves-*.tar.gz' -mtime +30 -delete
