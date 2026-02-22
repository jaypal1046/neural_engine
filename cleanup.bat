@echo off
echo ========================================
echo Optional Cleanup - Removing Redundant Files
echo ========================================
echo.

echo [1/2] Removing redundant server file...
if exist server\main_smart_brain.py (
    del server\main_smart_brain.py
    echo   ✓ Deleted server\main_smart_brain.py
) else (
    echo   - server\main_smart_brain.py already removed
)

echo.
echo [2/2] Removing Python cache...
for /d /r . %%d in (__pycache__) do @if exist "%%d" rd /s /q "%%d"
del /s /q *.pyc 2>nul
echo   ✓ Cleaned Python cache

echo.
echo ========================================
echo Cleanup complete!
echo ========================================
echo.
echo Main files kept:
echo   ✓ server\main.py                               (THE MAIN SERVER)
echo   ✓ desktop_app\src\components\HelperView.tsx   (THE MAIN CHAT)
echo   ✓ All C++ files in src/ and include/
echo   ✓ All documentation files
echo.
echo Smart Brain integration is now clean and consolidated!
echo.
