@echo off
chcp 65001 >nul
rem Banc d'essai du créateur de personnage : ouvre le studio en mode jeu (sans l'éditeur), enchaîne des réglages fixes
rem (modèles, visage, cheveux, cou, silhouettes) et enregistre les captures dans Saved\BancPerso\<étiquette>\, puis se ferme.
rem Usage : banc_perso.bat [étiquette]   (ex. banc_perso.bat avant). Fermer Unreal avant de lancer.

set "UE=D:\Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE%" set "UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "LABEL=%~1"
if "%LABEL%"=="" set "LABEL=essai"

"%UE%" "%~dp0..\..\PokemonNewAge.uproject" "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/Engine.GameModeBase" -game -windowed -ResX=1920 -ResY=1080 -ExecCmds="BancPerso %LABEL%" -BancQuit -log -unattended -nosplash
echo Captures : %~dp0..\..\Saved\BancPerso\%LABEL%
