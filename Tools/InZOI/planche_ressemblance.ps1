# Planche de ressemblance : chaque capture « 7NN_ressemblance » du banc BancPerso à côté de la vignette inZOI du modèle.
# Usage : powershell -File Tools\InZOI\planche_ressemblance.ps1 <étiquette> [7 = visages | 8 = silhouettes]  -> Saved\BancPerso\<étiquette>\planche_ressemblance.jpg
param([string]$Label = "essai", [string]$Prefix = "")

Add-Type -AssemblyName System.Drawing
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Dir = Join-Path $Root "Saved\BancPerso\$Label"
$Log = Join-Path $Root "Saved\Logs\BancPerso_$Label.log"
if (-not (Test-Path $Log)) { $Log = Join-Path $Root "Saved\Logs\PokemonNewAge.log" }
$Icons = "D:\Dev\SourceArt\CCMenu\Exercice Compilation\Output\Exports\BlueClient\Content\UI\Icon"

$Pairs = @()
foreach ($Line in Select-String -Path $Log -Pattern "\[BancPerso\] Vignette (\S+) = (\S+)") {
    $Name = $Line.Matches[0].Groups[1].Value
    $Asset = ($Line.Matches[0].Groups[2].Value -split "\.")[-1]
    $Icon = Get-ChildItem -Recurse $Icons -Filter "$Asset.png" -ErrorAction SilentlyContinue | Select-Object -First 1
    $Shot = Join-Path $Dir "$Name.png"
    if ($Icon -and (Test-Path $Shot) -and $Name.StartsWith($Prefix)) { $Pairs += , @($Shot, $Icon.FullName, $Name) }
}
if ($Pairs.Count -eq 0) { Write-Host "Aucune paire trouvée"; exit 1 }

# Capture : le visage est au centre-gauche de l'écran (studio décalé de 25 %) ; on recadre en carré autour
$Size = 360
$Cols = 2
$Out = New-Object Drawing.Bitmap ($Size * 2 * $Cols), ($Size * [Math]::Ceiling($Pairs.Count / $Cols))
$G = [Drawing.Graphics]::FromImage($Out)
$G.Clear([Drawing.Color]::White)
$G.InterpolationMode = 'HighQualityBicubic'
$Font = New-Object Drawing.Font 'Arial', 11
for ($I = 0; $I -lt $Pairs.Count; $I++) {
    $X = ($I % $Cols) * $Size * 2
    $Y = [Math]::Floor($I / $Cols) * $Size
    $Shot = [Drawing.Bitmap]::FromFile($Pairs[$I][0])
    # Visages (7xx) : gros plan ; silhouettes (8xx) : torse, du cou au haut des cuisses, comme les vignettes du corps
    $Crop = if ($Pairs[$I][2].StartsWith("8")) { New-Object Drawing.Rectangle 500, 240, 420, 420 } else { New-Object Drawing.Rectangle 300, 40, 1000, 1000 }
    $G.DrawImage($Shot, (New-Object Drawing.Rectangle $X, $Y, $Size, $Size), $Crop, 'Pixel')
    $Shot.Dispose()
    $Icon = [Drawing.Bitmap]::FromFile($Pairs[$I][1])
    $G.DrawImage($Icon, (New-Object Drawing.Rectangle ($X + $Size), $Y, $Size, $Size))
    $Icon.Dispose()
    $G.DrawString("$($Pairs[$I][2]) : jeu | inZOI", $Font, [Drawing.Brushes]::Red, $X + 4, $Y + 4)
}
$Path = Join-Path $Dir "planche_ressemblance$Prefix.jpg"
$Out.Save($Path, [Drawing.Imaging.ImageFormat]::Jpeg)
$G.Dispose(); $Out.Dispose()
Write-Host "Planche : $Path"
