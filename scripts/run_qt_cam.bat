@echo off
setlocal

set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_qt_cam.ps1" -StartBackend -RestartBackend %*

if errorlevel 1 (
    echo.
    echo CAMClaw Qt CAM failed to start.
    pause
)
