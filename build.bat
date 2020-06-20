@echo off

set PARAMS=%~1

del *.exe
type nul > output.log

cl -nologo -W3 -MDd -Od -EHsc %PARAMS% -Femain.exe main.cpp

if exist "main.exe" ( main.exe )

