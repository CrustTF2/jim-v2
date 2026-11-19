# Item Manager v2

Item Manager is a Windows application for viewing and organizing a Team Fortress 2 backpack. This project builds on [Jengerer's Item Manager](https://github.com/Jengerer/jim). This fork was developed with AI assistance, including help with code, testing, and documentation.

## Install

Download the Windows ZIP from this repository's Releases page and extract it to its own writable folder. Run `item_manager.exe` with Steam running and Team Fortress 2 installed. The application downloads item schema and image data on first use and keeps generated previews in its own folders. The release includes the item metadata needed for item display, but does not include a previously downloaded schema or cache. Python is not required to use the application.

War Paint and skin previews use an integrated native renderer. The renderer uses up to three workers and 512-pixel textures. The app waits for required previews to be generated while loading; it does not continue generating them in the background.

## Updates

The application checks the latest published release of this repository. When a newer release provides the expected executable asset, the user can choose to download and install it. The app verifies the downloaded executable against the SHA-256 digest supplied by GitHub. The current updater replaces **only `item_manager.exe`**; if a release changes supporting files, download and extract the full release ZIP as described in that release's notes. Your existing downloaded schema and caches are not included in the update.

## Build from source

Requirements: Windows, CMake 3.20 or newer, Visual Studio with the C++ desktop workload and the Windows SDK, PowerShell, and internet access to retrieve the pinned third-party sources. The target is 32-bit Windows.

```powershell
./prepare-dependencies.ps1
cmake -S . -B build -A Win32
cmake --build build --config Release --target item_manager decal_converter
```

The resulting program and bundled assets are in `build/package`. The dependency preparation script checks downloads against SHA-256 hashes in `dependency-sources.json`. The user-provided Steam Web API key is compiled into the application, so it is visible to anyone using or inspecting the distributed program.

## Credits

- Original application: [Jengerer/jim](https://github.com/Jengerer/jim), by Jengerer.
- User interface and utility libraries: [Jengerer/jui](https://github.com/Jengerer/jui) and [Jengerer/jutil](https://github.com/Jengerer/jutil).
- Steam interface definitions: [SteamRE/open-steamworks](https://github.com/SteamRE/open-steamworks).
- Dependencies: [Protocol Buffers](https://github.com/protocolbuffers/protobuf), [JsonCpp](https://github.com/open-source-parsers/jsoncpp), [zlib](https://github.com/madler/zlib), [libpng](https://github.com/pnggroup/libpng), [FreeType](https://github.com/freetype/freetype), [curl](https://github.com/curl/curl), and GLee. See `package/third-party-notices` for included notices.
- Team Fortress 2, Steam, related game assets and the Steamworks API belong to Valve. This project is not affiliated with or endorsed by Valve.
