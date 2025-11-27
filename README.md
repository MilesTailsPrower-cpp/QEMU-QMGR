# QEMU-QMGR
A simple VM manager for QEMU written in C++.

# Build instructions

**Linux (Debian/Ubuntu):**  
Step 1: Install dependencies  
Run `sudo apt-get install qt5-default cmake build-essential qemu-system-x86_64 qemu-utils -y`  
(For anyone running
Step 2: Download the source code and put it in a folder.  

Step 3: Build  
Create a folder named "build" and change directories to it: `mkdir build; cd build`  
Run cmake: `cmake ..`  
Build: `make -j$(nproc)`  

Run QMGR: `./qmgr`  
*(QEMU must already be installed; QMGR just launches it — no need to compile QEMU yourself.)*

---

**Windows (MinGW):**  
Step 1: Download and install MSYS2 and QEMU in the default folders.  

Step 1a: Install Qt5 via MSYS2 (way easier than the website!)  
- Open the MSYS2 MinGW 64-bit shell.  
- Update packages: `pacman -Sy` and then `pacman -Su`  
- Install Qt5 and MinGW toolchain: `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-qt5-base`  
- Optional: `pacman -S mingw-w64-x86_64-qt5-tools` if you want designer, linguist, etc.  

Step 2: Download the source code and put it in a folder.  

Step 3: Build  
- Open the MSYS2 MinGW 64-bit shell and go to your source folder.  
- Create a build folder: `mkdir build; cd build`  
- Run CMake: `cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/msys64/mingw64" ..`  
- Build: `cmake --build . --config Release`  

Run QMGR: `./qmgr.exe`  
*(QEMU must already be installed; QMGR just launches it.)*

---

# Usage

1. Launch QMGR.  
2. Click "Create VM" to make a new virtual machine.  
3. Set the disk image, memory, CPU, network/audio, and optionally enable VNC.  
4. Launch the VM — the QEMU window will appear.  
5. Use "Create Disk" to make new QCOW2 images.  
6. Export/import VM configurations as needed.  
7. Kill a running VM manually with the "Kill VM" button.

---

# Notes

- Make sure QEMU is installed and in your PATH.  
- No SDL2 or QEMU compilation required — QMGR only manages existing QEMU VMs.

---

# Dependencies
Since apparently we haven't evolved since the fucking stone age, on Linux you have to install dependencies manually. Run:
`sudo apt-get update;sudo apt-get install -y build-essential qtbase5-dev qt5-qmake qttools5-dev qttools5-dev-tools libx11-xcb-dev libglu1-mesa-dev libxcb-render-util0-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev libxcb-shm0-dev libxrender-dev libxi-dev qemu-utils qemu-system`
in a terminal.
On Windows, the ABSOLUTELY FUCKING RIDICULOUS AMOUNT OF DLLs are bundled with the executable. Extremely fucking stupid decision from Qt and GNU developers because I should be able to just have it in one executable, but NOOOOO, we have to do the stupidest things in the programming industry.
