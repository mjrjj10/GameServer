@echo off
REM A lancer UNE fois en administrateur sur le PC de chaque ami (pas sur le PC de l'hote)
net session >nul 2>&1 || (echo Clic droit ^> Executer en tant qu'administrateur & pause & exit /b 1)
netsh interface portproxy delete v4tov4 listenport=10900 listenaddress=127.0.0.1 >nul 2>&1
netsh interface portproxy add v4tov4 listenport=10900 listenaddress=127.0.0.1 connectport=10900 connectaddress=100.124.197.96
netsh interface portproxy show v4tov4
echo OK. Pour annuler : netsh interface portproxy delete v4tov4 listenport=10900 listenaddress=127.0.0.1
pause
