@echo off
rem Build and run the craft stage data / undo / save test
setlocal
pushd "%~dp0"
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist obj mkdir obj
cl /nologo /std:c++20 /EHsc /utf-8 /W3 /I..\..\project main.cpp ..\..\project\game\craft\CraftStage.cpp ..\..\project\game\craft\editor\CraftCommands.cpp /Fe:crafttest.exe /Fo:obj\ >build.log 2>&1
if errorlevel 1 ( type build.log & popd & exit /b 1 )
crafttest.exe
set RESULT=%ERRORLEVEL%
popd
exit /b %RESULT%
