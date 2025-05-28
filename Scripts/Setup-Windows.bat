@echo off

pushd ..
Vendor\premake\Windows\premake5.exe --file=Build.lua vs2022
popd
pause