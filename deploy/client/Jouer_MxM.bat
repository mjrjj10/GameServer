@echo off
cd /d "%~dp0"
if not exist MXMClient.exe copy MXMClient.bin MXMClient.exe >nul
set /p PSEUDO=Pseudo : 
start "" MXMClient.exe /LogEncryption /AuthMethod:local /Network:dev /PacketEncryption:0 /AutoJoinGame /AutoLoginID:%PSEUDO%
