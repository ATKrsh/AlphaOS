# build.ps1
# Build and Run script for AlphaOS.
# Compiles the assembly bootloader, C kernel, links them, combines them into a raw disk image, and runs it in QEMU.

$ErrorActionPreference = "Stop"

# Define Paths to Executables
$nasm = "C:\Users\atkrs\AppData\Local\bin\NASM\nasm.exe"
$clang = "C:\Program Files\LLVM\bin\clang.exe"
$lld = "C:\Program Files\LLVM\bin\ld.lld.exe"
$qemu = "C:\Program Files\qemu\qemu-system-i386.exe"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "         AlphaOS Build Pipeline         " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 1. Verify Tools Exist
$tools = @{
    "NASM Assembler" = $nasm
    "Clang Compiler" = $clang
    "LLD Linker"     = $lld
    "QEMU Emulator"  = $qemu
}

foreach ($toolName in $tools.Keys) {
    $path = $tools[$toolName]
    if (-not (Test-Path $path)) {
        Write-Error "Prerequisite tool '$toolName' not found at '$path'. Please ensure it is installed correctly."
    }
}
Write-Host "[*] All toolchain paths verified successfully." -ForegroundColor Green

# 2. Create Build Directory
$buildDir = "$PSScriptRoot/build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Write-Host "[*] Compiling Bootloader Assembly..." -ForegroundColor Yellow
# Assemble bootloader to a 512-byte flat binary
& $nasm -f bin "$PSScriptRoot/boot/boot.asm" -o "$buildDir/boot.bin"

# Verify boot.bin size
$bootSize = (Get-Item "$buildDir/boot.bin").Length
if ($bootSize -ne 512) {
    Write-Error "Bootloader size is not 512 bytes (actual: $bootSize bytes). Boot sector must be exactly 512 bytes!"
}

Write-Host "[*] Compiling Kernel Entry & Assembly Interrupts..." -ForegroundColor Yellow
# Assemble kernel entry to 32-bit ELF object
& $nasm -f elf32 "$PSScriptRoot/kernel/kernel_entry.asm" -o "$buildDir/kernel_entry.o"
# Assemble interrupt wrappers to 32-bit ELF object
& $nasm -f elf32 "$PSScriptRoot/kernel/interrupt.asm" -o "$buildDir/interrupt.o"

Write-Host "[*] Compiling Kernel C Modules..." -ForegroundColor Yellow
# Compile kernel C code targeting freestanding 32-bit ELF
& $clang -target i386-pc-none-elf -ffreestanding -m32 -fno-pie -fno-stack-protector -Os -c "$PSScriptRoot/kernel/kernel.c" -o "$buildDir/kernel.o"
& $clang -target i386-pc-none-elf -ffreestanding -m32 -fno-pie -fno-stack-protector -Os -c "$PSScriptRoot/kernel/idt.c" -o "$buildDir/idt.o"
& $clang -target i386-pc-none-elf -ffreestanding -m32 -fno-pie -fno-stack-protector -Os -c "$PSScriptRoot/kernel/isr.c" -o "$buildDir/isr.o"

Write-Host "[*] Linking Kernel..." -ForegroundColor Yellow
# Link objects together using linker.ld, generating a flat raw binary
& $lld -m elf_i386 -T "$PSScriptRoot/kernel/linker.ld" "$buildDir/kernel_entry.o" "$buildDir/interrupt.o" "$buildDir/idt.o" "$buildDir/isr.o" "$buildDir/kernel.o" -o "$buildDir/kernel.bin" --oformat binary

$kernelSize = (Get-Item "$buildDir/kernel.bin").Length
Write-Host "[*] Compilation and linkage completed." -ForegroundColor Green
Write-Host "    Bootloader size: $bootSize bytes" -ForegroundColor Gray
Write-Host "    Kernel size    : $kernelSize bytes" -ForegroundColor Gray

# 3. Concatenate and Pad OS Image
Write-Host "[*] Assembling bootable OS disk image (os-image.bin)..." -ForegroundColor Yellow
$bootBytes = [System.IO.File]::ReadAllBytes("$buildDir/boot.bin")
$kernelBytes = [System.IO.File]::ReadAllBytes("$buildDir/kernel.bin")

# Since our bootloader reads 32 sectors of kernel (32 * 512 = 16384 bytes)
# We must pad our image to contain the 512-byte bootloader + exactly 32 sectors of kernel space.
$sectorsToRead = 32
$kernelPadSize = $sectorsToRead * 512

if ($kernelBytes.Length -gt $kernelPadSize) {
    Write-Error "Kernel binary ($($kernelBytes.Length) bytes) is larger than the bootloader's sector read budget ($kernelPadSize bytes)! Increase sector count in boot.asm."
}

$totalSize = 512 + $kernelPadSize
$imageBytes = New-Object Byte[] $totalSize

# Copy bootloader and kernel into OS image
[System.Buffer]::BlockCopy($bootBytes, 0, $imageBytes, 0, 512)
[System.Buffer]::BlockCopy($kernelBytes, 0, $imageBytes, 512, $kernelBytes.Length)

# Write output file
[System.IO.File]::WriteAllBytes("$PSScriptRoot/os-image.bin", $imageBytes)
Write-Host "[+] Successfully created os-image.bin ($($imageBytes.Length) bytes)." -ForegroundColor Green

# 4. Run in QEMU
Write-Host "[*] Launching AlphaOS in QEMU..." -ForegroundColor Green
Write-Host "    Press 'Ctrl+Alt+G' inside QEMU to release cursor, or close the QEMU window to exit." -ForegroundColor Gray
& $qemu -drive format=raw,file="$PSScriptRoot/os-image.bin"
