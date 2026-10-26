# Direction artistique : Kanto 3D

Ticket : SCRUM-20 (planche d'ambiance « Kanto réaliste »).
Planche visuelle (images, palettes cliquables) : https://claude.ai/artifact/MxCsYpXrxK4N9NNAeFhvDo

Périmètre V1 : de Bourg Palette au badge de Pierre. Caméra à la 3e personne, derrière le joueur. Unreal Engine 5 avec Lumen.

## Les cinq règles

1. **Réaliste d'abord.** Matières réelles (bois, tuile, terre, pierre, tissu), éclairage de film. On simplifie seulement ce qui gêne la lecture du jeu.
2. **Couleurs franches.** Saturation un peu au-dessus du réel. L'herbe et les toits se lisent de loin.
3. **Lumière chaude.** Soleil doré, ombres bleutées et douces, beaucoup de ciel. Ambiance par défaut : un après-midi de vacances, façon Ghibli.
4. **Pokémon vivants.** Comme dans le film Détective Pikachu : vraie fourrure, vraies plumes, vraie peau, taille réelle. Silhouette et couleurs d'origine reconnaissables.
5. **Le jeu d'abord.** Hautes herbes, chemins, portes et dresseurs se repèrent du premier coup d'œil.

## Références

- Images partagées dans le projet : Bourg Palette en vue joueur (référence principale) et une forêt réaliste comparée à sa version 2002 (référence pour la Forêt de Jade).
- Détective Pikachu (film, 2019) : la référence pour les Pokémon réalistes.
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
- Humains aux proportions réelles (un ado de 1,60 m), pas des figurines à grosse tête.
- Taille fidèle au Pokédex (Pikachu 40 cm, Onix 8,8 m).
- À éviter : aspect plastique ou jouet, yeux d'anime, contours cel-shading.

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
| Bourg Palette de nuit, Boutique | À retoucher (maisons trop maquette, personnages figurines) |
| Centre Pokémon, labo du Prof. Chen, musée d'Argenta | À refaire (style anime ou cartoon) |
| Jadielle, Argenta, arène d'Argenta | À générer |
| Mont Sélénite | Après la V1 |

### Bloc de style à coller à la fin de chaque prompt

```
Style: photorealistic cinematic render in Unreal Engine 5, as if this place really existed. Real-world materials with visible detail (wood grain, worn fabric, stone, packed dirt, glass). Natural global illumination, warm late-afternoon sunlight, soft bluish shadows, light haze, subtle film color grading, 35 mm lens. Third-person camera behind the player at shoulder height. The trainer is a real teenager with realistic human proportions (about 1.60 m), real clothes and fabric folds, not a toy or cartoon figure. Pokémon look like real living creatures, as in the movie Detective Pikachu: realistic fur, feathers or skin, real-world scale, but their original silhouette and colors stay recognizable. Slightly saturated, cozy colors. Avoid: anime, cel shading, chibi, big heads, plastic or toy look, Nintendo Switch game style.
```
