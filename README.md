# JOndra

Emulátor počítače Ondra SPO 186.

Projekt obsahuje dva nezávislé buildy:

- Linux: CMake, systémové FLTK 1.3 a ALSA
- Windows 98 / Visual C++ 6: původní `Jondra.dsw` a `Jondra.dsp` s FLTK 1.1.10

## Linux – potřebné balíky

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake libfltk1.3-dev libasound2-dev
```

## Release build – kompatibilní s Ubuntu 18.04 / CMake 3.10

Nejjednodušší způsob:

```bash
./build-linux-release.sh
```

Počet paralelních úloh lze změnit:

```bash
JOBS=4 ./build-linux-release.sh
```

Ruční ekvivalent:

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . -- -j2
```

Spuštění:

```bash
cd build/release/bin
./Jondra
```

## Debug build – kompatibilní s CMake 3.10

```bash
mkdir -p build/debug
cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cmake --build . -- -j2
```

## Novější CMake

Na novějších distribucích lze použít také presety:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel 2
```

`CMakePresets.json` není určen pro CMake 3.10 z Ubuntu 18.04.

## Windows 98 / Visual C++ 6

Použij:

```text
Jondra.dsw
Jondra.dsp
```

Legacy Windows build používá FLTK 1.1.10. CMake build je určen jen pro Linux.

## Runtime soubory

Po sestavení CMake automaticky zkopíruje vedle programu:

```text
Jondra.config
images/
roms/
sound/
```
