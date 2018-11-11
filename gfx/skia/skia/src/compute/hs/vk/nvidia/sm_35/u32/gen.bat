::
:: Copyright 2018 Google Inc.
::
:: Use of this source code is governed by a BSD-style license that can
:: be found in the LICENSE file.
::

@ECHO OFF

::
:: delete the previous images
::

del *.comp
del *.spv
del *.xxd

::
::
::

set HS_GEN=..\..\..\..\..\..\spinel\bin\x64\Debug\hs_gen

:: --- 32-bit keys ---

:: %HS_GEN% -v -a "glsl" -D HS_NVIDIA_SM35 -t 1 -w 32 -r 32 -s 49152 -S 65536 -b 32 -m 1 -M 1 -f 1 -F 1 -c 1 -C 1 -z
%HS_GEN% -v -a "glsl" -D HS_NVIDIA_SM35 -t 1 -w 32 -r 16 -s 32768 -S 32768 -b 16 -m 1 -M 1 -p 1 -P 1 -f 0 -F 0 -c 0 -C 0 -z

::
:: remove trailing whitespace from generated files
::

sed -i 's/[[:space:]]*$//' hs_config.h
sed -i 's/[[:space:]]*$//' hs_modules.h

::
::
::

where glslangValidator

::
:: FIXME -- convert this to a bash script
::
:: Note that we can use xargs instead of the cmd for/do
::

for %%f in (*.comp) do (
    dos2unix %%f
    clang-format -style=Mozilla -i %%f                                   || goto :error
    cl -I ../.. -I ../../.. -EP %%f -P -Fi%%~nf.pre.comp                 || goto :error
    clang-format -style=Mozilla -i %%~nf.pre.comp                        || goto :error
    glslangValidator --target-env vulkan1.1 -o %%~nf.spv %%~nf.pre.comp  || goto :error
    spirv-opt -O %%~nf.spv -o %%~nf.spv                                  || goto :error
REM spirv-remap ...                                                      || goto :error
    xxd -i < %%~nf.spv > %%~nf.spv.xxd                                   || goto :error
    for /f %%A in ('wc -c %%~nf.spv') do (
        echo %%~nf.spv %%A
        printf "%%.8x" %%A | xxd -r -p | xxd -i > %%~nf.len.xxd          || goto :error
    )
)

::
:: dump a binary
::

cl -I ../../.. -I ../../../../.. /DHS_DUMP /Fe:hs_dump.exe *.c
hs_dump

::
:: delete temporary files
::

del *.pre.comp
del *.comp
del *.spv
REM del *.obj
REM del *.exe

exit /b 0

:error

exit /b %errorlevel%
