# EFZ Streaming Overlay

A lightweight DLL mod for Eternal Fighter Zero that provides real-time game data for OBS streaming overlays through a simple, file-based system.

This mod reads game data directly from memory and outputs it to local files, allowing for a robust, high-performance overlay with no need for web browsers or complex setup.

## Features

- Reads player win counts, nicknames, and character information directly from game memory.
- Supports both player and spectator modes by automatically detecting `EfzRevival.dll` for netplay data.
- Exports data to individual text files for easy integration with OBS Text Sources.
- Dynamically updates character portrait images for use with OBS Image Sources.
- Automatically creates and cleans up generated files on exit, keeping your game directory tidy.
- Provides an optional real-time console for logging and diagnostics.

## Getting Started

This guide is for using a pre-compiled release of the mod. If you want to build it yourself, see the [Building from Source](#building-from-source) section.

### Installation

This mod is designed to be loaded with the **EFZ Mod Manager**. You can download and install the EFZ Mod Manager here - [link](https://docs.google.com/spreadsheets/d/1r0nBAaQczj9K4RG5zAVV4uXperDeoSnXaqQBal2-8Us/edit?usp=sharing)

1.  **Download the latest release** from the [Releases](https://github.com/Aquat1c/efz_streaming/releases) page.
2.  From the downloaded `.zip` file, extract `efz_streaming_overlay.dll` and the `portraits` folder.
3.  Place `efz_streaming_overlay.dll` into your `(Game Directory)/mods/` folder.
4.  In your main game directory, open `EfzModManager.ini` in a text editor. Add the following line to the **bottom** of the file:
    ```ini
    efz_streaming_overlay=1
    ```
5.  Run the game once using the mod manager. The overlay DLL will load and create a new folder named `overlay_assets` inside your `mods` directory.
6.  Copy the `portraits` folder (from step 2) into the newly created `mods/overlay_assets/` directory.

Your final directory structure should look like this:
```
/Eternal Fighter Zero/
|-- efz.exe
|-- EfzModManager.ini
|-- mods/
|   |-- efz_streaming_overlay.dll
|   |-- overlay_assets/
|   |   |-- portraits/
|   |   |   |-- akane.png
|   |   |   |-- akiko.png
|   |   |   |-- ... (all other portrait files)
|   |   |-- p1_nickname.txt  (created by the mod)
|   |   |-- ... (all other text files)
```
The mod is now installed. The text and image files inside `overlay_assets` will update automatically while you play.

## OBS Integration

This setup uses native OBS sources for the best performance and stability.

### 1. Text Sources (Nicknames, Wins, etc.)
For each piece of text you want to display:
1.  In OBS, add a new **Text (GDI+)** source.
2.  In the source properties, check the box for **Read from file**.
3.  Click **Browse** and select the corresponding text file from the `mods/overlay_assets` directory (e.g., `p1_nickname.txt`).
4.  Style the font, color, and size as desired within OBS.

The available text files are:
- `p1_nickname.txt` / `p2_nickname.txt`
- `p1_character.txt` / `p2_character.txt`
- `p1_wins.txt` / `p2_wins.txt`

### 2. Image Sources (Character Portraits)
1.  In OBS, add a new **Image** source for Player 1.
2.  Click **Browse** and select the `p1_portrait.png` file from the `mods/overlay_assets` directory.
3.  Repeat for Player 2, adding another **Image** source and selecting `p2_portrait.png`.

The mod will automatically update these two image files when players select their characters, and OBS will display the changes instantly.

## Character Portraits

The mod uses the images provided in the `mods/overlay_assets/portraits/` folder to display the character art. A full set of portraits is included with each release.

If you wish to create your own custom portraits, you can edit the `.png` files, but you must keep the filenames identical. For reference, the required filenames are:

`akane.png`, `akiko.png`, `ikumi.png`, `misaki.png`, `sayuri.png`, `kanna.png`, `kaori.png`, `makoto.png`, `minagi.png`, `mio.png`, `mishio.png`, `misuzu.png`, `mizuka.png`, `nagamori.png`, `nanase.png`, `exnanase.png`, `nayuki.png`, `nayukib.png`, `shiori.png`, `ayu.png`, `mai.png`, `mayu.png`, `mizukab.png`, `kano.png`.

You must also include an `unknown.png` file, which is used as a default placeholder.

## Building from Source

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

## Troubleshooting

-   **Files are not being created in `overlay_assets`**:
    -   Ensure the mod is loaded correctly. If you have the console version, a window titled "EFZ Streaming Overlay - Debug Console" should appear.
    -   Check that your game directory is not in a write-protected location (like `C:\Program Files`) or if you're not trying to use the mod in the OneDrive directory.
-   **Character portraits show "unknown"(not the character)**:
    -   Make sure you have copied the `portraits` folder into the `mods/overlay_assets/` directory correctly.
    -   Verify that all portrait filenames are correct and have not been changed.

## License

This project is licensed under the MIT License.