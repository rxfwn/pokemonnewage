# Verre des menus (effet « Glass » de Figma)

Demandé par L'amie le 2026-11-01 : le même effet de verre que ses maquettes Figma (Options_Menu, Combat_Hud,
fenêtres de dialogue) sur les menus du jeu.

## Ce qui reprend quoi

| Figma | Unreal |
|---|---|
| Frost 4 (flou du décor) | widget **Background Blur** sous chaque panneau (force 6, `PokemonGlass::DefaultBlur`) |
| Remplissage dégradé #D9D9D9 39 % → #FFFFFF 13 % | matériau `M_UI_Glass`, paramètres `FillColorStart/End`, `FillOpacityStart/End` |
| Contour intérieur 2 px, blanc 1 % → 17 % | `StrokeWidth`, `StrokeOpacityStart/End`, `StrokeColor` |
| Opacité du calque 71 % | `LayerOpacity` (sur le verre seulement, pas sur le texte) |
| Lumière (reflet sur les bords) | `LightAngle` (0 = du haut), `LightIntensity` |
| Coins arrondis | `CornerRadius` (16 px, réglé par le C++) |
| Depth 24 / Refraction 100 | pas faisable en interface (un matériau d'UI ne peut pas lire l'image du jeu derrière lui) : le flou et le reflet des bords donnent l'essentiel |

Les dégradés suivent le CSS de Figma : `GradientAngle` en degrés (180 = de haut en bas, 90 = de gauche à droite), arrêts `GradientStart` / `GradientEnd` (0,4 % et 64,91 % → 0,004 et 0,6491).

## Fichiers

- `Tools/UI/make_glass_material.py` : fait `/Game/UI/Materials/M_UI_Glass` (domaine Interface utilisateur, translucide,
  un nœud Custom « Verre »). Lancé tout seul à l'ouverture de l'éditeur par `Content/Python/init_unreal.py` quand le
  matériau manque ou que sa version (`VERSION`) a changé.
- `Source/PokemonNewAge/UI/GlassPanel.h/.cpp` : `PokemonGlass::Make(...)` construit un panneau
  (Background Blur → image au matériau → contenu). À utiliser pour toute nouvelle fenêtre.
- Panneaux passés en verre : boîte de dialogue et choix Oui/Non, Paramètres, combat (cadres des Pokémon, texte,
  menu des choix, stats), Boutique, PC. Texte blanc avec une ombre légère, sélection en orange.

## Régler à l'œil

1. Dans le Navigateur de contenu, `/Game/UI/Materials/M_UI_Glass` : clic droit > **Créer une instance de matériau**
   (Create Material Instance), la nommer **`MI_UI_Glass`** dans le même dossier.
2. Ouvrir l'instance, cocher et changer les paramètres. Le jeu prend `MI_UI_Glass` en priorité s'il existe
   (relancer le jeu pour voir le changement). `CornerRadius` et `DpiScale` sont réglés par le C++.
3. Le flou se règle dans `GlassPanel.h` (`DefaultBlur`).
