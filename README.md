# EFZ Streaming Overlay

A lightweight DLL mod for Eternal Fighter Zero that provides real-time game data for OBS streaming overlays through a simple, file-based system.

This mod reads game data directly from memory and outputs it to local files, allowing for a robust, high-performance overlay with no need for web browsers or complex setup.

## Features

- Reads player win counts, nicknames, and character information directly from game memory.
- Supports both player and spectator modes by automatically detecting the correct memory offsets.
- Exports data to individual text files for easy integration with OBS Text Sources.
- Dynamically updates character portrait images for use with OBS Image Sources.
- Automatically creates and cleans up generated files on exit, keeping your game directory tidy.
- Provides a real-time console for logging and diagnostics.

## Building

### Requirements
- Visual Studio 2019 or later
- CMake 3.10+
- [Microsoft Detours](https://github.com/microsoft/detours) library (place in `3rdparty/detours/`)

### Build Steps
```bash
# Create a build directory
mkdir build
cd build

# Generate project files for 32-bit
cmake -A Win32 ..

# Build the Release configuration
cmake --build . --config Release
```
The final DLL will be located in `build/bin/Release/efz_streaming_overlay.dll`.

## Usage

1.  **Inject the DLL**: Use your preferred DLL injector to inject the `efz_streaming_overlay.dll` into the `efz.exe` process after the game has started.
2.  **Asset Folder**: The mod will automatically create an `overlay_assets` folder in the same directory as the DLL.
3.  **Add Portraits**: Inside `overlay_assets`, you will find a `portraits` folder. You must place your character portrait images (as `.png` files) in this folder. See the **Character Portraits** section below for the required filenames.

## OBS Integration (Recommended Method)

This setup uses native OBS sources for the best performance and stability.

### 1. Text Sources (Nicknames, Wins, etc.)
For each piece of text you want to display:
1.  In OBS, add a new **Text (GDI+)** source.
2.  In the source properties, check the box for **Read from file**.
3.  Click **Browse** and select the corresponding text file from the `overlay_assets` directory (e.g., `p1_nickname.txt`).
4.  Style the font, color, and size as desired within OBS.

The available text files are:
- `p1_nickname.txt` / `p2_nickname.txt`
- `p1_character.txt` / `p2_character.txt`
- `p1_wins.txt` / `p2_wins.txt`

### 2. Image Sources (Character Portraits)
1.  In OBS, add a new **Image** source for Player 1.
2.  Click **Browse** and select the `p1_portrait.png` file from the `overlay_assets` directory.
3.  Repeat for Player 2, adding another **Image** source and selecting `p2_portrait.png`.

The mod will automatically update these two image files when players select their characters, and OBS will display the changes instantly.

## Character Portraits

For the portrait hot-swapping to work, you must provide `.png` files in the `overlay_assets/portraits/` directory with the following exact filenames:

- `akane.png`
- `akiko.png`
- `ikumi.png`
- `misaki.png`
- `sayuri.png`
- `kanna.png`
- `kaori.png`
- `makoto.png`
- `minagi.png`
- `mio.png`
- `mishio.png`
- `misuzu.png`
- `mizuka.png`
- `nagamori.png`
- `nanase.png`
- `exnanase.png`
- `nayuki.png`
- `nayukib.png`
- `shiori.png`
- `ayu.png`
- `mai.png`
- `mayu.png`
- `mizukab.png`
- `kano.png`

Also you can include an `unknown.png` file, which will be used as a default if a character's portrait is not found.

## Troubleshooting

- **Console Errors**: If you encounter issues, run the game in windowed mode and check the console output for error messages.
- **Performance**: For best performance, close any unnecessary background applications and ensure your antivirus is not interfering with the DLL injection.
- **OBS Issues**: Make sure you are using the latest version of OBS, and try running OBS as an administrator.

## Changelog

### v1.0
- Initial release with core features: memory reading, file output, OBS integration.

### v1.1
- Added support for spectator mode.
- Improved file handling and cleanup.
- Enhanced error logging and diagnostics.

### v1.2
- Added character portrait hot-swapping feature.
- Optimized memory reading performance.
- Fixed various bugs and improved stability.


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
