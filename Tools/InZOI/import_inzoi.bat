@echo off
chcp 65001 >nul
rem Import des ressources inZOI SANS ouvrir l'éditeur et SANS carte graphique (-nullrhi) :
rem évite « Le GPU a planté ou le dispositif D3D a été retiré » pendant les longs imports.
rem Fermer Unreal avant de lancer. Double-cliquer sur ce fichier. Relançable : ce qui est fait est sauté.

rem Dossier d'Unreal 5.8 (à corriger s'il est installé ailleurs)
set "UE=D:\Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if not exist "%UE%" set "UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%UE%" (
  echo Unreal introuvable : %UE%
  echo Ouvre ce fichier avec le Bloc-notes et corrige la ligne "set UE=..."
  pause
  exit /b 1
)

"%UE%" "%~dp0..\..\PokemonNewAge.uproject" -run=pythonscript -script="%~dp0import_inzoi.py" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput
echo.
echo Import terminé (ou arrêté). Le bilan est dans les dernières lignes [InZOI] ci-dessus.
pause
