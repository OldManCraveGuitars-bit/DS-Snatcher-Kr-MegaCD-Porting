@echo off
cd /d "%~dp0"
py -3 -X utf8 -B patcher\patch_release.py
if errorlevel 1 echo Patching failed. Read the message above.
pause
