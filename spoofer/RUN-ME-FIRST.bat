@echo off
bcdedit /set testsigning on
certutil -addstore Root spoof.cer
certutil -addstore TrustedPublisher spoof.cer
echo.
echo DONE. Reboot now, then run loader.exe as Administrator and pick 1.
pause
