@echo off
rem Lance le jeu Pokemon apres l avoir mis a jour (docs/MISES_A_JOUR.md)
title Pokemon
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launcher.ps1" %*
if errorlevel 1 pause
