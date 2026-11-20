# Arma el .zip de Windows: lineage.exe, datos\, LICENSE.txt y LEEME.txt.
# Uso (desde la consola de desarrollador de Visual Studio, x64 o arm64; la arquitectura sale de ahí):
#   powershell -ExecutionPolicy Bypass -File herramientas\empaquetar_windows.ps1 [-Version 0.2.0]
# Deja dist\Lineage-<versión>-windows-<arq>.zip e imprime su ruta en la última línea. Lo usa
# .github/workflows/release.yml (y compilar.yml, que lo deja como artefacto en cada push).
#
# - Compila con el preset release (sin el control en vivo) en build\empaquetado-<arq>.
# - Runtime de C++ estático (/MT): se comprueba con dumpbin que los .exe no pidan VCRUNTIME ni MSVCP,
#   para que ande en una PC sin el redistribuible de Visual C++.
# - datos\ va junto al .exe: SDL_GetBasePath() devuelve la carpeta del ejecutable.
# - Sin firma de código: Windows SmartScreen avisa la primera vez ("Más información" > "Ejecutar de
#   todas formas"). Firmar queda pendiente (PLAN_PORTADO.md, Fase 4).
# - No incluye recursos del juego: el jugador elige su carpeta.
param([string]$Version = "0.0.0")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$arq = $env:VSCMD_ARG_TGT_ARCH  # x64 o arm64: lo define la consola de desarrollador (vcvarsall)
if (-not $arq) { throw "Correr desde la consola de desarrollador de Visual Studio (vcvarsall x64 o arm64)." }

$build = "build/empaquetado-$arq"
# Out-Host: la salida de cmake va a la consola y no al resultado del script (que es la ruta del .zip).
cmake --preset release -B $build "-DLIN_VERSION=$Version" | Out-Host
if ($LASTEXITCODE) { throw "cmake: falló la configuración" }
cmake --build $build --target lineage | Out-Host
if ($LASTEXITCODE) { throw "cmake: falló la compilación" }

# Sin DLL del runtime de Visual C++: solo las del sistema.
foreach ($exe in "lineage.exe") {
    $deps = dumpbin /nologo /dependents (Join-Path $build $exe) | Out-String
    if ($deps -match "(?i)(vcruntime|msvcp|ucrtbased|vcruntime\d+d)\w*\.dll") {
        throw "$exe depende del runtime de Visual C++ ($($Matches[0])): debería estar enlazado estático (/MT)"
    }
}

$nombre = "Lineage-$Version-windows-$arq"
$raiz = Join-Path "dist" $nombre
if (Test-Path $raiz) { Remove-Item -Recurse -Force $raiz }
New-Item -ItemType Directory -Force $raiz | Out-Null
Copy-Item (Join-Path $build "lineage.exe") $raiz
Copy-Item -Recurse datos (Join-Path $raiz "datos")
Copy-Item LICENSE (Join-Path $raiz "LICENSE.txt")
Copy-Item herramientas/windows/LEEME.txt $raiz

$zip = Join-Path "dist" "$nombre.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $raiz -DestinationPath $zip
Remove-Item -Recurse -Force $raiz
Write-Output $zip
