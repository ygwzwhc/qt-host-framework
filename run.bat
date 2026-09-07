@echo off
set "PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%"
set "QT_PLUGIN_PATH=C:\Qt\6.8.3\msvc2022_64\plugins"
start "" "%~dp0build\bin\qt_host_framework.exe"
