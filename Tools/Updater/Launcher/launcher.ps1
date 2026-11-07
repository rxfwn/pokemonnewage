# Launcher du jeu Pokémon (docs/MISES_A_JOUR.md)
#
# À chaque ouverture : demande au Raspberry Pi la dernière version, télécharge seulement les
# fichiers qui ont changé, puis lance le jeu. Sans connexion, lance la version déjà installée.
# Windows PowerShell 5.1 (déjà dans Windows 10 et 11) : rien à installer.
#
# Dossier du joueur :
#   Lancer Pokemon.bat      <- double-clic
#   launcher.ps1
#   launcher_config.json    {"server": "http://100.x.x.x:8788"}
#   Jeu\                    le jeu, rempli par le launcher
#
# Ce fichier doit rester en UTF-8 avec BOM, sinon Windows PowerShell affiche mal les accents.

param([Parameter(ValueFromRemainingArguments = $true)] [string[]] $GameArgs)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'Continue'
Add-Type -AssemblyName System.Net.Http

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigPath = Join-Path $Here 'launcher_config.json'
$GameDir = Join-Path $Here 'Jeu'
$StatePath = Join-Path $GameDir '.launcher_etat.json'

function Say([string] $Text, [string] $Color = 'Gray') { Write-Host $Text -ForegroundColor $Color }

function Fail([string] $Text) {
    Say ''
    Say $Text 'Red'
    exit 1
}

# ------------------------------------------------------------------ Réglages et état

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    '{ "server": "http://100.x.x.x:8788" }' | Set-Content -LiteralPath $ConfigPath -Encoding ASCII
    Fail "J'ai créé launcher_config.json : mets-y l'adresse Tailscale du Raspberry Pi, puis relance."
}
$Config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$Server = ([string] $Config.server).TrimEnd('/')
if (-not $Server.Contains('://')) { $Server = 'http://' + $Server }

function Load-State {
    $state = @{ version = ''; complete = $false; exe = ''; files = @{} }
    if (Test-Path -LiteralPath $StatePath) {
        try {
            $json = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
            $state.version = [string] $json.version
            $state.complete = [bool] $json.complete
            $state.exe = [string] $json.exe
            foreach ($p in $json.files.PSObject.Properties) {
                $state.files[$p.Name] = @{ size = [long] $p.Value.size; mtime = [long] $p.Value.mtime; sha256 = [string] $p.Value.sha256 }
            }
        } catch {
            Say "État du launcher illisible : je revérifie tous les fichiers." 'Yellow'
        }
    }
    return $state
}

function Save-State($state) {
    New-Item -ItemType Directory -Force -Path $GameDir | Out-Null
    $tmp = "$StatePath.tmp"
    ($state | ConvertTo-Json -Depth 5 -Compress) | Set-Content -LiteralPath $tmp -Encoding UTF8
    Move-Item -LiteralPath $tmp -Destination $StatePath -Force
}

function Start-Game($state) {
    $exe = Join-Path $GameDir ($state.exe -replace '/', '\')
    if (-not $state.exe -or -not (Test-Path -LiteralPath $exe)) {
        Fail "Le jeu n'est pas encore installé, et le serveur est injoignable. Vérifie que Tailscale est connecté, puis relance."
    }
    Say "Lancement du jeu (version $($state.version))..." 'Green'
    if ($GameArgs) {
        Start-Process -FilePath $exe -WorkingDirectory $GameDir -ArgumentList $GameArgs | Out-Null
    } else {
        Start-Process -FilePath $exe -WorkingDirectory $GameDir | Out-Null
    }
    exit 0
}

function Format-Size([double] $Bytes) {
    if ($Bytes -ge 1GB) { return '{0:N2} Go' -f ($Bytes / 1GB) }
    return '{0:N0} Mo' -f ($Bytes / 1MB)
}

$host.UI.RawUI.WindowTitle = 'Pokémon - mise à jour'
Say 'Pokémon : recherche d''une mise à jour...' 'Cyan'
$State = Load-State

# ------------------------------------------------------------------ Dernière version sur le Pi

$Manifest = $null
try {
    $quick = New-Object System.Net.Http.HttpClient
    $quick.Timeout = [TimeSpan]::FromSeconds(6)
    $text = $quick.GetStringAsync("$Server/manifest").GetAwaiter().GetResult()
    $Manifest = $text | ConvertFrom-Json
} catch {
    Say "Serveur de mises à jour injoignable ($Server)." 'Yellow'
    if (-not $State.complete) {
        if ($State.version) {
            Fail "La dernière mise à jour n'est pas finie : connecte Tailscale et relance pour la terminer."
        }
        Start-Game $State   # rien d'installé : affiche l'erreur
    }
    Say 'Je lance la version déjà installée.' 'Yellow'
    Start-Game $State
}

$Version = [string] $Manifest.version
if ($Manifest.note) { Say "Dernière version : $Version ($($Manifest.note))" } else { Say "Dernière version : $Version" }

# ------------------------------------------------------------------ Quels fichiers ont changé ?

$Wanted = @{}
foreach ($p in $Manifest.files.PSObject.Properties) { $Wanted[$p.Name] = $p.Value }

$ToGet = New-Object System.Collections.ArrayList
$NewFiles = @{}
$checked = 0
foreach ($rel in $Wanted.Keys) {
    $checked++
    $info = $Wanted[$rel]
    $local = Join-Path $GameDir ($rel -replace '/', '\')
    $ok = $false
    if (Test-Path -LiteralPath $local -PathType Leaf) {
        $item = Get-Item -LiteralPath $local
        $known = $State.files[$rel]
        $mtime = $item.LastWriteTimeUtc.Ticks
        if ($known -and $known.size -eq $item.Length -and $known.mtime -eq $mtime) {
            # Fichier pas touché depuis la dernière fois : pas besoin de le relire
            $ok = ($known.sha256 -eq $info.sha256)
            $sha = $known.sha256
        } elseif ($item.Length -eq [long] $info.size) {
            Write-Progress -Activity 'Vérification des fichiers' -Status $rel -PercentComplete ([int] (100 * $checked / $Wanted.Count))
            $sha = (Get-FileHash -LiteralPath $local -Algorithm SHA256).Hash.ToLower()
            $ok = ($sha -eq $info.sha256)
        }
        if ($ok) { $NewFiles[$rel] = @{ size = $item.Length; mtime = $mtime; sha256 = $sha } }
    }
    if (-not $ok) { [void] $ToGet.Add($rel) }
}
Write-Progress -Activity 'Vérification des fichiers' -Completed

# Fichiers d'une ancienne version qui n'existent plus dans la nouvelle
$ToDelete = @($State.files.Keys | Where-Object { -not $Wanted.ContainsKey($_) })

if ($ToGet.Count -eq 0 -and $ToDelete.Count -eq 0) {
    $State.version = $Version; $State.exe = [string] $Manifest.exe; $State.complete = $true; $State.files = $NewFiles
    Save-State $State
    Say 'Le jeu est à jour.' 'Green'
    Start-Game $State
}

$Total = [long] 0
foreach ($rel in $ToGet) { $Total += [long] $Wanted[$rel].size }
Say ("Mise à jour : {0} fichier(s) à télécharger, {1}." -f $ToGet.Count, (Format-Size $Total)) 'Cyan'

New-Item -ItemType Directory -Force -Path $GameDir | Out-Null
$drive = (Get-Item -LiteralPath $GameDir).PSDrive
if ($drive -and $drive.Free -lt ($Total + 500MB)) {
    Fail ("Pas assez de place sur le disque {0}: : il faut {1} de libre." -f $drive.Name, (Format-Size ($Total + 500MB)))
}

# ------------------------------------------------------------------ Téléchargement
# Chaque fichier va d'abord dans un .part à côté (reprise possible si on ferme la fenêtre),
# et les anciens fichiers ne sont remplacés qu'une fois TOUT téléchargé et vérifié.

$Client = New-Object System.Net.Http.HttpClient
$Client.Timeout = [System.Threading.Timeout]::InfiniteTimeSpan
$Buffer = New-Object byte[] (1MB)
$Done = [long] 0
$Watch = [System.Diagnostics.Stopwatch]::StartNew()
$LastShown = 0
$ResumedBytes = [long] 0
foreach ($rel in $ToGet) {
    $p = (Join-Path $GameDir ($rel -replace '/', '\')) + '.' + ([string] $Wanted[$rel].sha256).Substring(0, 12) + '.part'
    if (Test-Path -LiteralPath $p) { $ResumedBytes += [Math]::Min((Get-Item -LiteralPath $p).Length, [long] $Wanted[$rel].size) }
}
if ($ResumedBytes -gt 0) { Say ("Reprise : {0} déjà téléchargés la dernière fois." -f (Format-Size $ResumedBytes)) }

function Get-PartPath([string] $rel, [string] $sha) {
    return (Join-Path $GameDir ($rel -replace '/', '\')) + '.' + $sha.Substring(0, 12) + '.part'
}

function Download([string] $rel) {
    $info = $Wanted[$rel]
    $sha = [string] $info.sha256
    $size = [long] $info.size
    $part = Get-PartPath $rel $sha
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $part) | Out-Null

    $start = [long] 0
    if (Test-Path -LiteralPath $part) { $start = (Get-Item -LiteralPath $part).Length }
    if ($start -gt $size) { Remove-Item -LiteralPath $part -Force; $start = 0 }

    if ($start -lt $size -or $size -eq 0) {
        $request = New-Object System.Net.Http.HttpRequestMessage ([System.Net.Http.HttpMethod]::Get), "$Server/blobs/$sha"
        if ($start -gt 0) { $request.Headers.Range = New-Object System.Net.Http.Headers.RangeHeaderValue ($start, $null) }
        $response = $Client.SendAsync($request, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        if ([int] $response.StatusCode -eq 200) {
            $mode = [System.IO.FileMode]::Create
            $start = 0
        } elseif ([int] $response.StatusCode -eq 206) {
            $mode = [System.IO.FileMode]::Append
        } else {
            throw "le serveur répond $([int] $response.StatusCode) pour $rel"
        }
        $current = $start
        $in = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        $out = [System.IO.File]::Open($part, $mode, [System.IO.FileAccess]::Write)
        try {
            while (($n = $in.Read($Buffer, 0, $Buffer.Length)) -gt 0) {
                $out.Write($Buffer, 0, $n)
                $current += $n
                $shown = $script:Done + $current
                if ($Watch.ElapsedMilliseconds - $script:LastShown -gt 400) {
                    $script:LastShown = $Watch.ElapsedMilliseconds
                    $speed = ($shown - $script:ResumedBytes) / [Math]::Max(0.5, $Watch.Elapsed.TotalSeconds)
                    $left = if ($speed -gt 0) { [int] (($Total - $shown) / $speed) } else { -1 }
                    Write-Progress -Activity "Téléchargement de la version $Version" `
                        -Status ("{0} / {1} ({2:N1} Mo/s)" -f (Format-Size $shown), (Format-Size $Total), ($speed / 1MB)) `
                        -CurrentOperation $rel -PercentComplete ([int] [Math]::Min(100, 100 * $shown / [Math]::Max(1, $Total))) `
                        -SecondsRemaining $left
                }
            }
        } finally {
            $out.Close(); $in.Close(); $response.Dispose()
        }
    }

    $got = (Get-FileHash -LiteralPath $part -Algorithm SHA256).Hash.ToLower()
    if ($got -ne $sha) {
        Remove-Item -LiteralPath $part -Force
        throw "fichier abîmé pendant le téléchargement : $rel"
    }
    $script:Done += $size
}

foreach ($rel in $ToGet) {
    $tries = 0
    while ($true) {
        try { Download $rel; break }
        catch {
            $tries++
            if ($tries -ge 4) {
                Write-Progress -Activity 'Téléchargement' -Completed
                Fail "Téléchargement interrompu ($($_.Exception.Message)). Relance : il reprendra là où il s'est arrêté."
            }
            Say "  coupure ($($_.Exception.Message)), nouvel essai dans 3 s..." 'Yellow'
            Start-Sleep -Seconds 3
        }
    }
}
Write-Progress -Activity 'Téléchargement' -Completed

# ------------------------------------------------------------------ Remplacement

# À partir d'ici, le jeu n'est plus « complet » tant que tout n'est pas remplacé
# (jusque-là, l'ancienne version reste jouable hors ligne)
$State.complete = $false
$State.version = $Version
Save-State $State

foreach ($rel in $ToGet) {
    $info = $Wanted[$rel]
    $local = Join-Path $GameDir ($rel -replace '/', '\')
    try {
        Move-Item -LiteralPath (Get-PartPath $rel ([string] $info.sha256)) -Destination $local -Force
    } catch {
        Fail "Impossible de remplacer $rel : le jeu est peut-être encore ouvert. Ferme-le, puis relance."
    }
    $item = Get-Item -LiteralPath $local
    $NewFiles[$rel] = @{ size = $item.Length; mtime = $item.LastWriteTimeUtc.Ticks; sha256 = [string] $info.sha256 }
}
foreach ($rel in $ToDelete) {
    $local = Join-Path $GameDir ($rel -replace '/', '\')
    if (Test-Path -LiteralPath $local) { Remove-Item -LiteralPath $local -Force -ErrorAction SilentlyContinue }
}
# Morceaux de téléchargements abandonnés (version publiée entre deux essais)
Get-ChildItem -LiteralPath $GameDir -Recurse -Filter '*.part' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

$State.version = $Version; $State.exe = [string] $Manifest.exe; $State.complete = $true; $State.files = $NewFiles
Save-State $State
Say ("Mise à jour terminée ({0} en {1:N0} s)." -f (Format-Size $Total), $Watch.Elapsed.TotalSeconds) 'Green'
Start-Game $State
