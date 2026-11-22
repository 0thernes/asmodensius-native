@echo off
rem AZST viewer. Open http://127.0.0.1:8766/ after startup.
setlocal
set "VIEWER_BUILD=%~dp0..\build\z"
if not exist "%VIEWER_BUILD%\az_live.exe" set "VIEWER_BUILD=%~dp0..\..\b"
python -I -B "%~dp0viewer_server.py" --engine "%VIEWER_BUILD%\az_live.exe" --data-dir "%VIEWER_BUILD%\viewer-data" %*
exit /b %errorlevel%
