@ECHO OFF

set TESS_BIN=bin64

start %TESS_BIN%\imprimis.exe "-u$HOME\My Games\Imprimis" -glog.txt %*
