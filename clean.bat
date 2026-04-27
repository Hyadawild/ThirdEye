@echo off
echo Cleaning build files...

del /Q *.obj 2>nul
del /Q *.sys 2>nul
del /Q *.dll 2>nul
del /Q *.lib 2>nul
del /Q *.exp 2>nul
del /Q *.pdb 2>nul
del /Q *.ilk 2>nul

rmdir /S /Q ..\driver\obj 2>nul
rmdir /S /Q ..\driver\Debug 2>nul
rmdir /S /Q ..\driver\Release 2>nul

echo Clean complete.
pause