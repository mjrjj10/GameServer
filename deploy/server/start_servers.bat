@echo off
cd /d "%~dp0"
start "MxM matchmaker" mm_srv.exe
timeout /t 2 /nobreak >nul
start "MxM hub" hub_srv.exe
start "MxM game" game_srv.exe
timeout /t 2 /nobreak >nul
start "MxM login" login_srv.exe
