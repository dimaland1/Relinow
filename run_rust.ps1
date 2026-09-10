Write-Host "=============================================="
Write-Host "FLASH DU BENCHMARK RUST (PARTITION 3MB)"
Write-Host "=============================================="
$port = Read-Host "Sur quel port voulez-vous flasher ? (Appuyez sur Entrée pour COM4, ou tapez COM6)"
if ([string]::IsNullOrWhiteSpace($port)) { $port = "COM4" }

$binDir = "examples/benchmark_rust/target/xtensa-esp32-espidf/debug"
$bootloader = "$binDir/bootloader.bin"
$partitions = "examples/benchmark_rust/partition-table-3mb.bin"
$elfFile = "$binDir/benchmark_rust"
$binFile = "$binDir/benchmark_rust.bin"
if (Test-Path $elfFile) {
    Write-Host "Mise à jour de l'image .bin à partir du fichier ELF..."
    python -m esptool --chip esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB -o $binFile $elfFile
}

Write-Host ""
Write-Host "IMPORTANT: Dès que esptool affiche 'Connecting...', appuyez et MAINTENEZ"
Write-Host "le bouton 'BOOT' sur la carte ($port) jusqu'à ce que l'écriture commence !"
Write-Host ""
Read-Host "Appuyez sur ENTRÉE pour démarrer le flash sur $port"

python -m esptool --chip esp32 --port $port --baud 460800 write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 $bootloader 0x8000 $partitions 0x10000 $binFile

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Flash réussi sur $port !"
    $mon = Read-Host "Voulez-vous ouvrir le moniteur série sur $port ? (o/N)"
    if ($mon -eq "o" -or $mon -eq "O") {
        python -m serial.tools.miniterm $port 115200
    }
}


