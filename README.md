# JOndra

## Česky

### Linux

Potřebné balíky pro Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libfltk1.3-dev libasound2-dev libpng-dev
```

Na Debianu Squeeze použij místo `libfltk1.3-dev` balík `libfltk1.1-dev`.

Sestavení pomocí skriptu:

```bash
./build-linux-release.sh
```

Ruční sestavení pomocí CMake:

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j2
```

### Windows 98

Pro Windows 98 použij Visual C++ 6.0 a FLTK 1.1.10. Projekt otevři pomocí:

```text
Jondra.dsw
```

Případně lze samostatně otevřít:

```text
Jondra.dsp
```

## English

### Linux

Required packages for Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libfltk1.3-dev libasound2-dev libpng-dev
```

On Debian Squeeze use `libfltk1.1-dev` instead of `libfltk1.3-dev`.

Build using the script:

```bash
./build-linux-release.sh
```

Manual CMake build:

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j2
```

### Windows 98

For Windows 98 use Visual C++ 6.0 and FLTK 1.1.10. Open the project with:

```text
Jondra.dsw
```

Alternatively, open the project file directly:

```text
Jondra.dsp
```
