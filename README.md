# Item Manager v2

Item Manager is a Windows and Linux application for viewing and organizing a Team Fortress 2 backpack. This project builds on [Jengerer's Item Manager](https://github.com/Jengerer/jim). This fork was developed with AI assistance, including help with code, testing, and documentation.

## Install

Download the appropriate archive from this repository's Releases page and extract it to its own writable folder. Run the app with Steam running and Team Fortress 2 installed. The application downloads item schema and image data on first use and keeps generated previews in its own folders. The release includes the item metadata needed for item display, but does not include a previously downloaded schema or cache.

**Windows:** Extract the Windows archive and run `item_manager.exe`. War Paint and skin previews use an integrated native renderer with up to three workers and 512-pixel textures.

**Linux:** The build targets x86-64 Ubuntu-based systems and requires a graphical desktop, OpenGL, native Linux Steam, and TF2. Extract the Linux archive to a writable location, open a terminal in the extracted folder, and run:

```
bash install-system-dependencies.sh
bash setup-renderer.sh
bash run-item-manager.sh
```

The Linux archive includes a private Python runtime for its War Paint renderer; the setup script creates a virtual environment and downloads the required Python packages. If shared libraries are missing, run `bash install-system-dependencies.sh` and retry. This helper uses APT to install the required graphics and networking libraries and may request your administrator password. The renderer uses 512-pixel textures and adjusts the worker count to available CPUs, up to three. For a single launch, `JIM_PREVIEW_WORKERS=3 bash run-item-manager.sh` overrides the worker count.

On both platforms, the app waits for required previews to be generated while loading.

## Updates

Updates are installed by downloading the new archive. Your existing downloaded schema and caches are not included in the update.

## Build from source

The platform-specific sources are in `windows/` and `linux/`.

Before building either version, download a v2.0.2 release archive and copy its `particles` folder to the repository root, renaming it `particle-assets`. Alternatively, prepare that folder from the original `.webp` files using `tools/prepare_particles.py` and Python with Pillow installed.

**Windows:** Requirements are CMake 3.20 or newer, Visual Studio with the C++ desktop workload and the Windows SDK, PowerShell, and internet access to retrieve pinned third-party sources. The target is 32-bit Windows.

```powershell
cd windows
./prepare-dependencies.ps1
cmake -S . -B build -A Win32
cmake --build build --config Release --target item_manager decal_converter
```

The resulting program and bundled assets are in `build/package`. The dependency preparation script checks downloads against SHA-256 hashes in `dependency-sources.json`.

**Linux:** Install a C++17 compiler, CMake, Python 3, and the SDL2, OpenGL, GLU, FreeType, libpng, zlib, and libcurl development packages. On Ubuntu-based systems:

```sh
sudo apt install build-essential cmake python3 libsdl2-dev libgl-dev libglu1-mesa-dev libfreetype-dev libpng-dev zlib1g-dev libcurl4-openssl-dev
cd linux
sh prepare-dependencies.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target item-manager --parallel
cd build/package
bash setup-renderer.sh
bash run-item-manager.sh
```

The Linux build downloads pinned Protocol Buffers and JsonCpp sources and verifies their checksums. A source build uses an installed Python 3.10–3.12 for the renderer; the downloadable Linux release includes its own runtime.

## Credits

- Original application: [Jengerer/jim](https://github.com/Jengerer/jim), by Jengerer.
- User interface and utility libraries: [Jengerer/jui](https://github.com/Jengerer/jui) and [Jengerer/jutil](https://github.com/Jengerer/jutil).
- Steam interface definitions: [SteamRE/open-steamworks](https://github.com/SteamRE/open-steamworks).
- Dependencies: [Protocol Buffers](https://github.com/protocolbuffers/protobuf), [JsonCpp](https://github.com/open-source-parsers/jsoncpp), [zlib](https://github.com/madler/zlib), [libpng](https://github.com/pnggroup/libpng), [FreeType](https://github.com/freetype/freetype), [curl](https://github.com/curl/curl), and GLee. See `windows/package/third-party-notices` for included notices.
- Linux uses [SDL2](https://github.com/libsdl-org/SDL), [Python Build Standalone](https://github.com/astral-sh/python-build-standalone), and the Python renderer dependencies listed in `linux/thumbnail_renderer/requirements.txt`.
- Unusual Effect Images/Animations: [Equip.tf](https://equip.tf).
- Team Fortress 2, Steam, related game assets and the Steamworks API belong to Valve. This project is not affiliated with or endorsed by Valve.
