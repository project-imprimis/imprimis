@ECHO OFF

set TESS_BIN=bin64

start %TESS_BIN%\tesseract.exe "-u$HOME\My Games\Imprimis" -glog.txt %*
