# Relief v3 : des choses à voir au loin

Le relief de `L_Kanto` part maintenant du terrain sculpté par L'amie dans Unreal (exporté le 2026-11-03). Le script `Tools/Blockout/relief_vues.py` le transforme pour casser le plat, sans tout sculpter à la main.

## Ce que fait le script

Le script part de deux fichiers de L'amie :
- `Tools/Blockout/reference/relief_amie_2026-11-03.png` : sa heightmap exportée d'Unreal (échelle Z 200 au moment de l'export) ;
- `Tools/Blockout/reference/kanto_relief_amie.png` : sa carte de Kanto (FRLG posée sur un relief peint, nord en haut). Le brun, le gris et le blanc donnent les montagnes, le vert foncé les forêts, le bleu à l'intérieur des terres les lacs.

Il applique ensuite, dans l'ordre :
1. **Nettoyage** : les bosses rondes du pinceau sont fondues, le trou à -512 m est rebouché.
2. **Bords ondulés** : les lignes droites (bords des couloirs FRLG, côtes) ondulent de ± 28 m, sauf au cœur des routes et dans les villes.
3. **Villes contrastées** : Argenta à 200 m, Lavanville à 150 m, Safrania à 85 m, Azuria à 70 m. Les autres gardent leur hauteur. Le terrain suit sur 650 m autour, en pente douce.
4. **Collines** : des collines douces partout sauf en ville, et des collines boisées qui grandissent loin des routes. Les rangées d'arbres FRLG ne comptent pas comme des routes.
5. **Montagnes** : celles de la carte de L'amie, plus celles qu'elle a entourées en rouge (`EXTRA_MOUNTAINS`), un volcan sur Cramois'Île, le Mont Argenté et des îles au loin. Rien ne dépasse le Plateau Indigo, une table plate à 640 m, le point le plus haut.
6. **Érosion** : par la pluie, sur les nouveaux reliefs seulement.
7. **Chemins doux** : sur les routes, le relief est lissé puis limité à 15 % de pente (`PATH_MAX_SLOPE`). Le raccord avec le reste se fait sur 60 m, en courbe.
8. **Eau** : une dizaine de lacs arrondis sont creusés, et l'eau creusée par L'amie devient un lit peu profond aux berges douces. La cascade de la Route 23 est creusée dans la falaise, avec un bassin au pied.

Les rebords à sauter (on saute d'un côté, on fait le tour de l'autre) ne sont **pas** dans le relief. Ce seront des objets (Blueprint) posés à la main.

## Lancer le script

```
pip install numpy numba scipy pillow
python Tools/Blockout/relief_vues.py --test DOSSIER   # aperçus rapides (vue du dessus), ~3 min
python Tools/Blockout/relief_vues.py                  # heightmap complète, ~6 min
python Tools/Blockout/apercu_vue_sol.py DOSSIER/relief.npy DOSSIER/vue   # vues au sol et d'avion
```

La heightmap complète remplace `Tools/Blockout/kanto_v1_heightmap.png`. Les réglages sont en haut du script (hauteurs, pentes, lacs, montagnes).

## Importer dans Unreal

**L'échelle Z passe de 200 à 300**, pour que le Plateau Indigo puisse monter à 640 m (avec 200, on plafonnait à 512 m).

1. Ouvrir `L_Kanto`, mode **Paysage (Landscape) > Gestion (Manage) > Importer (Import)**.
2. Choisir **Importer**, cocher **Fichier de la texture de hauteur (Heightmap File)** et prendre `Tools/Blockout/kanto_v1_heightmap.png`.
3. **Mode d'importation : All**, puis cliquer sur **Importer**.
4. Sélectionner le Landscape dans l'Outliner. Dans **Détails > Transformation (Transform) > Échelle (Scale)**, mettre **Z = 300** (X et Y restent à 150).
5. Charger le terrain : **World Partition > Load Region From Selection**, sinon la vue est noire.

Attention :
- Réimporter remplace tout le sculpt fait depuis l'export du 2026-11-03. Pour retoucher plus tard, sculpter dans un calque d'édition à part.
- Les scripts qui lisent le relief (`blockout_kanto_v1.py`, `place_teleport_points.py`, `place_grass_zones.py`) lisent l'échelle dans `kanto_v1_layout.LANDSCAPE_SCALE_Z`, déjà passée à 300.
