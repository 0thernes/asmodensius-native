@echo off
rem Asmodensius-Native live demo: double-click to start, then open http://127.0.0.1:8765/
rem The server listens on this computer only. Close this window to stop it.
title Asmodensius-Native live demo (close to stop)
cd /d "%~dp0"
python -I "%~dp0live_server.py" --open %*
if errorlevel 1 pause
