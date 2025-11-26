# QEMU-QMGR
A simple VM manager for QEMU written in C++.

# Build instructions

**Linux (Debian/Ubuntu):**  
Step 1: Install dependencies  
Run `sudo apt-get install qt5-default cmake build-essential qemu-system-x86_64 qemu-utils libsdl2-dev -y`  

Step 2: Download the source code and put it in a folder.  

Step 3: Build  
Create a folder named "build" and change directories to it: `mkdir build; cd build`  
Run cmake: `cmake ..`  
Build: `make -j$(nproc)`  

Run QMGR: `./qmgr`

---

**Windows (MinGW):**  
Step 1: Download and install MSYS2, Qt5, SDL2, and QEMU in the default folders. Open MSYS2.  

Step 2: Download the source code and put it in a folder.  

Step 3: Build  
Open the MSYS2 MinGW 64-bit shell and go to your source folder.  
Create a build folder: `mkdir build; cd build`  
Run CMake (replace the path if needed): `cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt5/mingw_64" ..`  
Build: `cmake --build . --config Release`  

Step 4: Copy the required Qt DLLs next to qmgr.exe: Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, Qt5Network.dll, Qt5PrintSupport.dll  

Run QMGR: `./qmgr.exe`

---

# Usage

1. Launch QMGR.  
2. Click "Create VM" to make a new virtual machine.  
3. Set the disk image, memory, CPU, network/audio, and optionally enable VNC.  
4. Launch the VM; the SDL window will appear.  
5. Use "Create Disk" to make new QCOW2 images.  
6. Export/import VM configurations as needed.  
7. Kill a running VM manually with the "Kill VM" button.

---

# Notes

- Make sure QEMU is installed and in your PATH.  
- SDL2 is required for the VM window.  
- On Windows, copy all necessary Qt DLLs to the same folder as qmgr.exe.  

---

# License

MIT License — free to use, modify, and distribute.
