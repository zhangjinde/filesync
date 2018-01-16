
@echo off

if "%1%"=="" (
    echo "Please input {x86|x64} makefile_path"
    goto tail
)

if "%2%"=="" (
    echo "Please input {x86|x64} makefile_path"
    goto tail
)

if "%1%"=="x86" (
    path D:\backup\VC\Tools\MSVC\14.12.25827\bin\Hostx86\x86\
) else (
    path D:\backup\VC\Tools\MSVC\14.12.25827\bin\Hostx64\x64\
)

cd %2

nmake /f makefile.txt

:tail

pause
