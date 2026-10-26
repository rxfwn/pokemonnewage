# Direction artistique : Kanto 3D

Ticket : SCRUM-20 (planche d'ambiance « Kanto réaliste »).
Planche visuelle (images, palettes cliquables) : https://claude.ai/artifact/MxCsYpXrxK4N9NNAeFhvDo

Périmètre V1 : de Bourg Palette au badge de Pierre. Caméra à la 3e personne, derrière le joueur. Unreal Engine 5 avec Lumen.

## Les cinq règles

1. **Réaliste, mais chaleureux.** Lumière et matières crédibles, comme un film d'animation 3D haut de gamme. Kanto reste propre, lumineux et accueillant : jamais sale, terne ou abandonné.
2. **Couleurs franches.** Saturation un peu au-dessus du réel. L'herbe et les toits se lisent de loin.
3. **Lumière chaude.** Soleil doré, ombres bleutées et douces, beaucoup de ciel. Ambiance par défaut : un après-midi de vacances, façon Ghibli.
4. **Pokémon vivants et mignons.** Fourrure ou peau douce et crédible, mais forme, couleurs et bouille officielles. Plus vivants que dans les jeux Switch, jamais inquiétants.
5. **Le jeu d'abord.** Hautes herbes, chemins, portes et dresseurs se repèrent du premier coup d'œil.

## Références

- Images partagées dans le projet : Bourg Palette en vue joueur (référence principale) et une forêt réaliste comparée à sa version 2002 (référence pour la Forêt de Jade).
- Détective Pikachu (film, 2019) : pour la matière des Pokémon, sans ses visages trop réalistes.
- Légendes Pokémon : Arceus (Pokémon dans une nature ouverte), Écarlate et Violet (rendu des Pokémon, villes).
- Zelda : Tears of the Kingdom (paysage simplifié mais riche, lisibilité des chemins).
- Mon voisin Totoro (campagne, ciel d'été) et Kiki la petite sorcière (ville lumineuse, piste pour Jadielle).
- Fortnite (Lumen et Nanite sur un rendu stylisé coloré).

## Palettes par lieu

Points de départ pour les matériaux et l'éclairage.

| Lieu | Couleurs |
|---|---|
| Bourg Palette | Tuile `#B5523A`, bois miel `#C98E4F`, barrière `#F4F1E8`, herbe `#6DAE3E`, terre `#A97B52`, ciel `#7DB7E8` |
| Route 1 | Herbe haute `#3F7F2F`, prairie `#8CC152`, talus `#8A6A45`, fleur `#F2C94C`, fleur rose `#E88BA6`, ombre `#3D5A73` |
| Forêt de Jade | Canopée `#1F4D2B`, mousse `#5E8C3A`, écorce `#4A3A2A`, rayon `#F3D58A`, brume `#A9C4B5`, Chenipan `#9BC53D` |
| Jadielle | Toit vert `#3E7D5A`, crépi `#EFE3C8`, pavé `#9C958A`, Centre Pokémon `#D9443A`, Boutique `#3C79C2`, arbre `#4E8A3A` |
| Argenta | Granite `#8E8A82`, pierre claire `#C9C1B1`, ardoise `#55595E`, sable `#D8C29A`, bois sombre `#6B4A32`, arène `#7A7266` |
| Mont Sélénite | Roche `#3A3946`, nuit `#1E2238`, cristal `#A8D8F0`, lune `#E8E6F4`, lichen `#6E8F7A`, lampe `#F0A35A` |
| Intérieurs | Parquet `#B07A4A`, mur `#F2E9D8`, tapis `#C2483D`, comptoir `#E7E2DA`, rouge Centre `#E2554A`, lampe `#FFD7A0` |
| Bourg Palette de nuit | Ciel `#1C2547`, ombre `#2E3B5E`, lune `#C9D6F2`, fenêtre `#FFB35C`, herbe nuit `#2F4A3A`, lanterne `#F08A3C` |

## Lumière

Réglages de départ (Directional Light, Sky Atmosphere, Exponential Height Fog, Post Process), à ajuster à l'œil.

| Heure | Où | Soleil | Température | Brume |
|---|---|---|---|---|
| Matin | Route 1 | 12° | 4 500 K | 0.03 |
| Après-midi (défaut) | Bourg Palette, Jadielle, Argenta | 40°, de trois quarts | 5 600 K | 0.01, nuages épars |
| Coucher | Retour au labo, cinématiques | 4° | 3 200 K | 0.04, orangée |
| Nuit | Bourg Palette de nuit | Lune, faible | 7 500 K (lune), 2 700 K (fenêtres) | 0.02, bleue |

Base commune : Lumen, Sky Atmosphere, Volumetric Clouds, exposition fixe (pas d'auto-exposition), bloom doux, vignette légère, saturation globale +10 %.

## Matériaux et formes

**Décors**
- Textures réalistes mais peu bruitées.
- Bords arrondis, proportions un peu trapues.
- Surfaces mates. Le brillant est réservé à l'eau, aux vitres et aux Poké Balls.
- Végétation dense, animée par le vent, plus verte que nature.
- À éviter : usure et saleté trop poussées, gris neutres et couleurs délavées.

**Pokémon et personnages**
- Base : modèles des jeux Switch, repassés dans Blender puis retexturés (fourrure, plumes, peau) avant l'export FBX.
- Humains aux proportions naturelles (un ado de 1,60 m), légèrement stylisés comme dans l'image du combat sauvage.
- Taille fidèle au Pokédex (Pikachu 40 cm, Onix 8,8 m).
- À éviter : aspect plastique ou jouet, yeux d'anime, contours cel-shading, visages trop réalistes.

**Lisibilité**
- Hautes herbes plus foncées et plus hautes que l'herbe normale.
- Chemins en terre claire qui guident vers la sortie de chaque zone.
- Portes et entrées éclairées ou encadrées de couleur.
- Une couleur dominante par arène (gris pierre pour Argenta).

## Images d'ambiance

Le 2026-10-26, la direction a été poussée vers le réalisme : les premières images Gemini faisaient trop « jeu Switch », surtout les personnages. Les images sont rangées dans le dossier partagé du projet `ambiance/`.

| Scène | Statut |
|---|---|
| Combat sauvage | Validée, référence de réalisme |
| Route 1, chambre, salon, sas route/ville, arène de Jadielle fermée | Validées |
| Centre Pokémon | Validée, refaite avec le bloc de style v2 (exemple du bon équilibre) |
| Bourg Palette de nuit | Validée (v3 : labo blanc avec antenne au bout du chemin, mer derrière) |
| Boutique | Validée, refaite avec le bloc de style v2 |
| Labo du Prof. Chen, musée d'Argenta | Validées, refaites avec le bloc de style v2 |
| Jadielle | Validée (v2, à taille réelle : préciser « true human scale, no miniature, no tilt-shift » dans le prompt) |
| Argenta | Validée |
| Arène d'Argenta | Validée |
| Mont Sélénite | Après la V1 |

### Harmoniser les images

1. Joindre au prompt Gemini l'image `ambiance/combat-sauvage.jpg` (ou `chambre.jpg` pour un intérieur) comme référence de style.
2. Coller ce bloc à la fin du prompt. Version 2 : la première poussait trop vers le photoréalisme et donnait des lieux sales et ternes.

```
Style: high-end cinematic 3D render, like a modern animated feature film with realistic lighting, matching the style of the attached reference image. Places look real and well kept: clean, bright, cozy, freshly painted, with believable materials (wood, tiles, stone, fabric). Warm golden sunlight, soft shadows, cheerful saturated colors. Third-person camera behind the player. The trainer is a teenager with natural proportions, slightly stylized like in the reference image. Pokémon are appealing and cute, with soft realistic fur or skin, and keep their official shape, colors and friendly face. Avoid: dirt, grime, decay, run-down or abandoned places, gloomy or desaturated colors, uncanny realistic faces, anime, cel shading, toy or plastic look.
```
