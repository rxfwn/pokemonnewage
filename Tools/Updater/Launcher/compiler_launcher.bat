@echo off
rem Fabrique Pokemon.exe (le launcher) avec le compilateur C# fourni dans Windows : rien a installer.
rem Double-clic sur ce fichier, puis donner Pokemon.exe a chaque joueur (docs/MISES_A_JOUR.md).
setlocal
cd /d "%~dp0"
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" (
  echo Compilateur introuvable : il faut le .NET Framework 4, deja present dans Windows 10 et 11.
  if not "%~1"=="ci" pause
  exit /b 1
)
"%CSC%" /nologo /codepage:65001 /target:winexe /optimize+ /out:Pokemon.exe /win32icon:pokemon.ico /r:System.Web.Extensions.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll Launcher.cs
if errorlevel 1 (
  echo Echec de la compilation.
  if not "%~1"=="ci" pause
  exit /b 1
)
echo Pokemon.exe est pret dans %~dp0
if not "%~1"=="ci" pause
