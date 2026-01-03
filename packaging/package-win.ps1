################################################################################
# package-win.ps1 - Build and package XenAdmin Qt for Windows
################################################################################
# This script builds XenAdmin Qt as a static executable for Windows.
# Requirements:
# - Qt6 with static build (or MSVC dynamic build + windeployqt)
# - Visual Studio Build Tools or MinGW
# - PowerShell 5.1 or later
################################################################################

# Hint how to build static Qt on windows
# 1) Get MSYS2 from https://www.msys2.org/
# 2) Install required packages:
#     pacman -S --needed base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja git python
# 3) Get Qt source code:
#     git clone https://code.qt.io/qt/qt5.git
# 4) Build it
#     target_path=G:/Qt/6.9.3-static-mingw
#     ./init-repository
#     ./configure -platform win32-g++ -static -release -opensource -confirm-license -nomake tests -nomake examples -no-icu -opengl desktop -skip qtwebengine -prefix "$target_path" -skip qtdoc 
#     cmake --build . 
#     cmake --install .

param(
    [string]$QtPath = "",
    [string]$MingwPath = "",
    [string]$Version = "0.0.1",
    [string]$BuildType = "Release",
    [string]$Compiler = "mingw",  # msvc or mingw
    [int]$Jobs = 0,
    [switch]$Static = $true,
    [switch]$Help
)

# Show help
if ($Help) {
    Write-Host @"
Usage: package-win.ps1 [options]

Options:
  -QtPath <path>       Path to Qt installation (e.g., C:\Qt\6.5.0\msvc2019_64)
  -MingwPath <path>    Path to MinGW bin directory (e.g., C:\msys64\mingw64\bin)
  -Version <version>   Application version (default: 0.0.1)
  -BuildType <type>    Build type: Release or Debug (default: Release)
  -Compiler <type>     Compiler: msvc, mingw, or llvm-mingw (default: mingw)
  -Jobs <n>            Number of parallel build jobs (default: number of CPUs)
  -Static              Build static executable (requires Qt static build)
  -Help                Show this help message

Examples:
  # Build with Qt from default path
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64

  # Build static release version
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64_static -Static -Version 1.0.0

  # Build with MinGW
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\mingw_64 -Compiler mingw

  # Build with separate MinGW installation
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\mingw_64 -MingwPath C:\msys64\mingw64\bin -Compiler mingw

  # Build with LLVM-MinGW (modern Clang-based toolchain)
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\llvm-mingw_64 -Compiler llvm-mingw

  # Build with custom LLVM-MinGW installation
  .\package-win.ps1 -QtPath C:\Qt\6.5.0\qt_64 -MingwPath C:\llvm-mingw\bin -Compiler llvm-mingw

"@
    exit 0
}

# Set error action preference
$ErrorActionPreference = "Stop"

# Load config values
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigFile = Join-Path $ScriptDir "config"

# Default values (will be overridden by config file if present)
$AppName = "xenadmin-qt"
$AppIconPath = "src/xenadmin-ui/images/AppIcon.ico"
$Maintainer = "Petr Bena <petr@bena.rocks>"
$Description = "XenServer/XCP-ng management UI"

# Parse config file (simple bash variable extraction)
if (Test-Path $ConfigFile) {
    Get-Content $ConfigFile | ForEach-Object {
        if ($_ -match '^APP_NAME="([^"]+)"') { 
            $AppName = $Matches[1] 
        }
        if ($_ -match '^APP_VERSION="([^"]+)"' -and $Version -eq "0.0.1") { 
            $Version = $Matches[1] 
        }
        if ($_ -match '^MAINTAINER="([^"]+)"') { 
            $Maintainer = $Matches[1] 
        }
        if ($_ -match '^DESCRIPTION="([^"]+)"') { 
            $Description = $Matches[1] 
        }
        if ($_ -match '^APP_ICON_PATH="([^"]+)"') { 
            $AppIconPath = $Matches[1] 
        }
    }
} else {
    Write-Host "Warning: Config file not found at $ConfigFile, using default values" -ForegroundColor Yellow
}

# Determine number of CPU cores
if ($Jobs -eq 0) {
    $Jobs = (Get-CimInstance -ClassName Win32_ComputerSystem).NumberOfLogicalProcessors
}

Write-Host "=================================" -ForegroundColor Cyan
Write-Host "Building XenAdmin Qt for Windows" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan
Write-Host "Version:     $Version"
Write-Host "Build Type:  $BuildType"
Write-Host "Compiler:    $Compiler"
Write-Host "Static:      $Static"
Write-Host "CPU Cores:   $Jobs"
Write-Host ""

# Validate Qt path
if (-not $QtPath) {
    Write-Host "Error: Qt path is required. Use -QtPath to specify Qt installation." -ForegroundColor Red
    Write-Host "Example: -QtPath C:\Qt\6.5.0\msvc2019_64" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $QtPath)) {
    Write-Host "Error: Qt path does not exist: $QtPath" -ForegroundColor Red
    exit 1
}

# Setup Qt environment
$QtBinPath = Join-Path $QtPath "bin"
if (-not (Test-Path $QtBinPath)) {
    Write-Host "Error: Qt bin directory not found: $QtBinPath" -ForegroundColor Red
    exit 1
}

# Add Qt to PATH
$env:Path = "$QtBinPath;$env:Path"

# Verify qmake is available
$QmakeCmd = "qmake.exe"
if (-not (Get-Command $QmakeCmd -ErrorAction SilentlyContinue)) {
    Write-Host "Error: qmake.exe not found in Qt bin path: $QtBinPath" -ForegroundColor Red
    exit 1
}

Write-Host "Qt Path:     $QtPath" -ForegroundColor Green
Write-Host "qmake:       $(Get-Command qmake.exe | Select-Object -ExpandProperty Source)" -ForegroundColor Green
Write-Host ""

# Get project root directory (parent of packaging/)
$ProjectRoot = Split-Path -Parent $ScriptDir
$SrcDir = Join-Path $ProjectRoot "src"
$BuildDir = Join-Path $ProjectRoot "release"
$OutputDir = Join-Path $ScriptDir "output"

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# Setup compiler environment
Write-Host "Step 0: Setting up compiler environment..." -ForegroundColor Cyan
Write-Host ""

if ($Compiler -eq "msvc") {
    # Find Visual Studio installation
    $VsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    
    if (Test-Path $VsWherePath) {
        $VsPath = & $VsWherePath -latest -property installationPath
        
        if ($VsPath) {
            # Look for vcvarsall.bat
            $VcVarsAll = Join-Path $VsPath "VC\Auxiliary\Build\vcvarsall.bat"
            
            if (Test-Path $VcVarsAll) {
                Write-Host "Found Visual Studio: $VsPath" -ForegroundColor Green
                
                # Determine architecture
                $Arch = "x64"
                if ($QtPath -match "win32" -or $QtPath -match "_32") {
                    $Arch = "x86"
                }
                
                # Run vcvarsall and capture environment
                Write-Host "Initializing MSVC environment for $Arch..." -ForegroundColor Yellow
                $TempFile = [System.IO.Path]::GetTempFileName()
                
                cmd /c "`"$VcVarsAll`" $Arch && set > `"$TempFile`""
                
                Get-Content $TempFile | ForEach-Object {
                    if ($_ -match "^(.*?)=(.*)$") {
                        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
                    }
                }
                
                Remove-Item $TempFile
                Write-Host "MSVC environment initialized" -ForegroundColor Green
            } else {
                Write-Host "Warning: vcvarsall.bat not found, assuming environment is already set up" -ForegroundColor Yellow
            }
        }
    } else {
        Write-Host "Warning: vswhere.exe not found, assuming MSVC environment is already set up" -ForegroundColor Yellow
    }
    
    $MakeCmd = "nmake.exe"
    if (-not (Get-Command $MakeCmd -ErrorAction SilentlyContinue)) {
        Write-Host "Error: nmake.exe not found. Please run from Visual Studio Developer Command Prompt." -ForegroundColor Red
        exit 1
    }
} elseif ($Compiler -eq "mingw") {
    # Add MinGW to PATH if specified
    if ($MingwPath) {
        if (Test-Path $MingwPath) {
            $env:Path = "$MingwPath;$env:Path"
            Write-Host "Added MinGW to PATH: $MingwPath" -ForegroundColor Green
        } else {
            Write-Host "Error: MinGW path does not exist: $MingwPath" -ForegroundColor Red
            exit 1
        }
    }
    
    # Try mingw32-make.exe first, then make.exe
    if (Get-Command "mingw32-make.exe" -ErrorAction SilentlyContinue) {
        $MakeCmd = "mingw32-make.exe"
    } elseif (Get-Command "make.exe" -ErrorAction SilentlyContinue) {
        $MakeCmd = "make.exe"
    } else {
        Write-Host "Error: Neither mingw32-make.exe nor make.exe found in PATH" -ForegroundColor Red
        if ($MingwPath) {
            Write-Host "Searched in: $MingwPath" -ForegroundColor Yellow
        } else {
            Write-Host "Searched in: Qt bin path ($QtBinPath)" -ForegroundColor Yellow
        }
        Write-Host "Please use -MingwPath to specify MinGW bin directory (e.g., C:\msys64\mingw64\bin)" -ForegroundColor Yellow
        exit 1
    }
} elseif ($Compiler -eq "llvm-mingw") {
    # LLVM-MinGW: Modern Clang-based MinGW toolchain used by Qt 6.x
    # Add LLVM-MinGW to PATH if specified
    if ($MingwPath) {
        if (Test-Path $MingwPath) {
            $env:Path = "$MingwPath;$env:Path"
            Write-Host "Added LLVM-MinGW to PATH: $MingwPath" -ForegroundColor Green
        } else {
            Write-Host "Error: LLVM-MinGW path does not exist: $MingwPath" -ForegroundColor Red
            exit 1
        }
    }
    
    # Verify clang is available
    if (-not (Get-Command "clang.exe" -ErrorAction SilentlyContinue)) {
        Write-Host "Error: clang.exe not found in PATH" -ForegroundColor Red
        Write-Host "LLVM-MinGW toolchain requires clang compiler" -ForegroundColor Yellow
        if ($MingwPath) {
            Write-Host "Searched in: $MingwPath" -ForegroundColor Yellow
        } else {
            Write-Host "Searched in: Qt bin path ($QtBinPath)" -ForegroundColor Yellow
        }
        Write-Host "Please use -MingwPath to specify LLVM-MinGW bin directory" -ForegroundColor Yellow
        exit 1
    }
    
    # Try various make commands (llvm-mingw typically uses mingw32-make or make)
    if (Get-Command "mingw32-make.exe" -ErrorAction SilentlyContinue) {
        $MakeCmd = "mingw32-make.exe"
    } elseif (Get-Command "make.exe" -ErrorAction SilentlyContinue) {
        $MakeCmd = "make.exe"
    } elseif (Get-Command "ninja.exe" -ErrorAction SilentlyContinue) {
        $MakeCmd = "ninja.exe"
        Write-Host "Note: Using Ninja build system" -ForegroundColor Cyan
    } else {
        Write-Host "Error: No suitable build tool found (mingw32-make.exe, make.exe, or ninja.exe)" -ForegroundColor Red
        if ($MingwPath) {
            Write-Host "Searched in: $MingwPath" -ForegroundColor Yellow
        } else {
            Write-Host "Searched in: Qt bin path ($QtBinPath)" -ForegroundColor Yellow
        }
        exit 1
    }
    
    Write-Host "Using Clang: $(Get-Command clang.exe | Select-Object -ExpandProperty Source)" -ForegroundColor Green
    Write-Host "Using build tool: $MakeCmd" -ForegroundColor Green
} else {
    Write-Host "Error: Unknown compiler: $Compiler (must be 'msvc', 'mingw', or 'llvm-mingw')" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Build the application
Write-Host "Step 1: Building application..." -ForegroundColor Cyan
Write-Host ""

# Create/clean build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir

try {
    # Run qmake
    Write-Host "Running qmake..." -ForegroundColor Yellow
    
    $QmakeArgs = @()
    if ($Static) {
        $QmakeArgs += "CONFIG+=static"
    }
    
    Push-Location $SrcDir
    & qmake.exe @QmakeArgs xenadminqt.pro -o "$BuildDir\Makefile"
    Pop-Location
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: qmake failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "qmake completed successfully" -ForegroundColor Green
    Write-Host ""
    
    # Build with nmake or mingw32-make
    Write-Host "Building with $MakeCmd (using $Jobs parallel jobs)..." -ForegroundColor Yellow
    Write-Host ""
    
    # Redirect stderr to stdout and capture all output without truncation
    if ($Compiler -eq "msvc") {
        # nmake doesn't support -j directly, use /MAXCPUCOUNT for msbuild or just run nmake
        & cmd /c "$MakeCmd 2>&1"
    } else {
        # MinGW make and LLVM-MinGW support -j
        & cmd /c "$MakeCmd -j $Jobs 2>&1"
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "Error: Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Build completed successfully" -ForegroundColor Green
    Write-Host ""
    
    # Check if binary was created
    $BinaryPath = Join-Path $BuildDir "xenadmin-ui\$BuildType\xenadmin-qt.exe"
    if (-not (Test-Path $BinaryPath)) {
        # Try without BuildType subdirectory
        $BinaryPath = Join-Path $BuildDir "xenadmin-ui\xenadmin-qt.exe"
    }
    
    if (-not (Test-Path $BinaryPath)) {
        Write-Host "Error: Built executable not found at expected location" -ForegroundColor Red
        Write-Host "Searched: $BuildDir\xenadmin-ui\$BuildType\xenadmin-qt.exe" -ForegroundColor Yellow
        Write-Host "      and $BuildDir\xenadmin-ui\xenadmin-qt.exe" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "Binary built: $BinaryPath" -ForegroundColor Green
    
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Step 2: Deploying Qt dependencies..." -ForegroundColor Cyan
Write-Host ""

# Create deployment directory
$DeployDir = Join-Path $OutputDir "${AppName}-${Version}-windows"
if (Test-Path $DeployDir) {
    Remove-Item -Recurse -Force $DeployDir
}
New-Item -ItemType Directory -Path $DeployDir | Out-Null

# Copy executable
Copy-Item $BinaryPath -Destination (Join-Path $DeployDir "xenadmin-qt.exe")

if (-not $Static) {
    # Use windeployqt for dynamic builds
    $WinDeployQt = Join-Path $QtBinPath "windeployqt.exe"
    
    if (Test-Path $WinDeployQt) {
        Write-Host "Running windeployqt..." -ForegroundColor Yellow
        
        Push-Location $DeployDir
        
        $BuildTypeArg = "--" + $BuildType.ToLower()
        & $WinDeployQt $BuildTypeArg --no-translations xenadmin-qt.exe
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Warning: windeployqt failed with exit code $LASTEXITCODE" -ForegroundColor Yellow
        } else {
            Write-Host "windeployqt completed successfully" -ForegroundColor Green
        }
        
        Pop-Location
    } else {
        Write-Host "Warning: windeployqt.exe not found, you may need to manually copy Qt DLLs" -ForegroundColor Yellow
    }
} else {
    Write-Host "Static build - no Qt DLLs needed" -ForegroundColor Green
}

Write-Host ""
Write-Host "Step 3: Adding additional files..." -ForegroundColor Cyan
Write-Host ""

# Copy README and LICENSE
$ReadmePath = Join-Path $ProjectRoot "README.md"
if (Test-Path $ReadmePath) {
    Copy-Item $ReadmePath -Destination $DeployDir
}

$LicensePath = Join-Path $ProjectRoot "LICENSE"
if (Test-Path $LicensePath) {
    Copy-Item $LicensePath -Destination $DeployDir
}

Write-Host ""
Write-Host "Step 4: Creating distribution archive..." -ForegroundColor Cyan
Write-Host ""

# Create ZIP archive
$ZipName = "${AppName}-${Version}-windows.zip"
$ZipPath = Join-Path $OutputDir $ZipName

if (Test-Path $ZipPath) {
    Remove-Item $ZipPath
}

# Use .NET compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($DeployDir, $ZipPath)

Write-Host "Archive created: $ZipPath" -ForegroundColor Green

# Get file size
$ZipSize = (Get-Item $ZipPath).Length / 1MB
Write-Host "Archive size: $([math]::Round($ZipSize, 2)) MB" -ForegroundColor Green

Write-Host ""
Write-Host "=================================" -ForegroundColor Cyan
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Package:     $ZipPath" -ForegroundColor Green
Write-Host "Directory:   $DeployDir" -ForegroundColor Green
Write-Host ""
Write-Host "To install:" -ForegroundColor Yellow
Write-Host "  1. Extract $ZipName to a directory" -ForegroundColor White
Write-Host "  2. Run xenadmin-qt.exe" -ForegroundColor White
Write-Host ""

# Verify executable
$ExePath = Join-Path $DeployDir "xenadmin-qt.exe"
$ExeSize = (Get-Item $ExePath).Length / 1MB
Write-Host "Executable size: $([math]::Round($ExeSize, 2)) MB" -ForegroundColor Cyan

if ($Static) {
    Write-Host "Build type: Static (monolithic .exe)" -ForegroundColor Green
} else {
    Write-Host "Build type: Dynamic (requires Qt DLLs)" -ForegroundColor Yellow
}

Write-Host ""
