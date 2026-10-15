@echo off
netsh advfirewall firewall delete rule name="MxM server (Tailscale)" >nul 2>&1
netsh advfirewall firewall add rule name="MxM server (Tailscale)" dir=in action=allow protocol=TCP localport=10900,11900,12900 remoteip=100.64.0.0/10
pause
