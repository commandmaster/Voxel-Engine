@echo off

pushd ..
Vendor\bin\premake\Windows\premake5.exe --file=Build.lua vs2022
popd
pause