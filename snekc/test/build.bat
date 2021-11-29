@echo off

mkdir bin
snekc src/**.c -build bin
clang bin/*.ll -o bin/a.exe -glldb -target x86_64-pc-windows-msvc19.29.30133 -w "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.18362.0/um/x64/kernel32.lib" lib/glfw3dll.lib
