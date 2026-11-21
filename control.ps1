# Menu local para las funciones verificadas del receptor 2.4 GHz.
$ErrorActionPreference = 'Stop'
$bin = Join-Path $PSScriptRoot 'build\Release'
$rgb = Join-Path $bin 'rgb_prueba.exe'
$battery = Join-Path $bin 'bateria_f75.exe'
if (-not (Test-Path -LiteralPath $rgb) -or -not (Test-Path -LiteralPath $battery)) {
    Write-Host 'Faltan los ejecutables. Compila primero con cmake --build build --config Release.'
    exit 1
}
function Read-Channel([string] $name) {
    while ($true) {
        $value = Read-Host "$name (0-255, o Q para cancelar)"
        if ($value -eq 'q') { return $null }
        $number = 0
        if ([int]::TryParse($value, [ref]$number) -and $number -ge 0 -and $number -le 255) { return $number }
        Write-Host 'Introduce un numero entero entre 0 y 255.'
    }
}
while ($true) {
    Write-Host "`n=== Control AULA 2.4 GHz ==="
    Write-Host '1. Consultar bateria'
    Write-Host '2. Elegir color RGB fijo'
    Write-Host '3. Salir'
    $option = Read-Host 'Elige una opcion'
    switch ($option) {
        '1' { & $battery --consultar; if ($LASTEXITCODE -ne 0) { Write-Host 'No se pudo obtener una respuesta valida.' } }
        '2' {
            $r = Read-Channel 'Rojo'; if ($null -eq $r) { break }
            $g = Read-Channel 'Verde'; if ($null -eq $g) { break }
            $b = Read-Channel 'Azul'; if ($null -eq $b) { break }
            & $rgb "$r" "$g" "$b" --aplicar
            if ($LASTEXITCODE -ne 0) { Write-Host 'No se completo el envio.' }
        }
        '3' { exit 0 }
        default { Write-Host 'Opcion no valida.' }
    }
}
