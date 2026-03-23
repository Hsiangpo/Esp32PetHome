@echo off
setlocal

set "HVIGOR_BAT="
if not defined JAVA_HOME (
  set "JAVA_HOME=D:\Develop\IDE\DevEco Studio\jbr"
)
if exist "%JAVA_HOME%\bin\java.exe" (
  set "PATH=%JAVA_HOME%\bin;%PATH%"
)

if defined DEVECO_STUDIO_HOME (
  set "HVIGOR_BAT=%DEVECO_STUDIO_HOME%\tools\hvigor\bin\hvigorw.bat"
)

if not defined HVIGOR_BAT if defined DEVECO_SDK_HOME (
  set "HVIGOR_BAT=%DEVECO_SDK_HOME%\..\tools\hvigor\bin\hvigorw.bat"
)

if not defined HVIGOR_BAT (
  set "HVIGOR_BAT=D:\Develop\IDE\DevEco Studio\tools\hvigor\bin\hvigorw.bat"
)

if exist "%HVIGOR_BAT%" (
  call "%HVIGOR_BAT%" --no-daemon %*
  exit /b %ERRORLEVEL%
)

hvigor %*
exit /b %ERRORLEVEL%
