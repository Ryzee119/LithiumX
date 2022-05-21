# LithiumX

A simple dashboard, mainly developed for the Original Xbox console, but it can be compiled for Windows and Linux for rapid development and testing.

<img  src="./images/dash_main.jpg" alt="main" width="50%"/>

## Features
* Customisable search paths and pages.
* Supports game synopsis information and boxart using the [XBMC4Gamers artwork format](https://github.com/Rocky5/XBMC4Gamers/blob/master/README.md#game-resources-and-synopsis).
* Keeps track of recently launched titles to quickly get back into your games.
* Will run at 720p if available, otherwise it will automatically fallback to 480p.
* FTP Server (Xbox build only)
* GPU Accelerated

## Controls
* Black/White - Change page
* LT/RT - Scroll page
* D-PAD - Select title
* Back - Show synopsis screen
* Start - Show main menu
* A - Launch selected title

## Settings
* On the first launch, a `dash.xml` will be created alongside the default.xbe with a starting template. Edit this to modify search paths for titles.
* The settings file only supports ASCII characters.
* The settings file does not support comments or other text.
* If the template is invalid, the program will reset it back to the inbuilt default.

## Todo
- [ ] Take advantage of 128MB Xbox RAM mods.
- [ ] Some basic audio.
- [ ] File browser.
- [ ] Lots more testing.

## Images
<img  src="./images/dash_main.jpg" alt="main" width="75%"/>
<img  src="./images/dash_menu.jpg" alt="menu" width="75%"/>
<img  src="./images/dash_synop.jpg" alt="recent" width="75%"/>
<img  src="./images/dash_recent.jpg" alt="synopsis" width="75%"/>

## Build (Original Xbox)
Setup and install nxdk, then:
```
sudo apt-get update -y && sudo apt-get install -y flex bison clang lld llvm
git clone --recursive https://github.com/Ryzee119/LithiumX.git
cd LithiumX
./src/lib/nxdk/bin/activate
make -f Makefile.nxdk -j (Add -B if editing lv_conf.h or other header files to ensure its built correctly)
```

## Build (Windows Xbox)
Install MYSYS2, then from a mingw64 environment:
```
pacman -Syu
pacman -S mingw-w64-x86_64-make \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-libjpeg-turbo

git clone --recursive https://github.com/Ryzee119/LithiumX.git
cd LithiumX/
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Build (Linux)
```
sudo apt-get update
sudo apt install build-essential libsdl2-dev libturbojpeg-dev
git clone --recursive https://github.com/Ryzee119/LithiumX.git
cd LithiumX/
mkdir build && cd build
cmake ..
cmake --build .
```

## Licence and Attribution
This project is shared under the [MIT license](https://github.com/Ryzee119/LithiumX/blob/master/LICENSE), however this project includes code by others. Refer to the list below.
* [lvgl](https://github.com/lvgl)/**[lvgl](https://github.com/lvgl/lvgl)** shared under the [MIT License](https://github.com/lvgl/lvgl/blob/master/LICENCE.txt).
* [charlesnicholson](https://github.com/charlesnicholson)/**[nanoprintf](https://github.com/charlesnicholson/nanoprintf)** shared under the [MIT License](https://github.com/charlesnicholson/nanoprintf/blob/main/LICENSE).
* [ooxi](https://github.com/ooxi/)/**[xml.c](https://github.com/ooxi/xml.c)** shared under the [MIT License](https://github.com/ooxi/xml.c/blob/master/LICENSE).
* [XboxDev](https://github.com/XboxDev)/**[nxdk](https://github.com/XboxDev/nxdk)** shared under the [Various Licenses](https://github.com/XboxDev/nxdk/tree/master/LICENSES).
* [sandertrilectronics](https://github.com/sandertrilectronics)/**[LWIP-FreeRTOS-FTP-Server](https://github.com/sandertrilectronics/LWIP-FreeRTOS-Netconn-FTP-Server)** shared under the [Apache 2.0 License](https://github.com/Ryzee119/LithiumX/blob/master/src/lib/ftpd/LICENSE).
