# EFZ Streaming Overlay

A DLL mod for Eternal Fighter Zero that provides real-time game data for OBS streaming overlays.

## Features

- Reads player win counts, nicknames, and character information from game memory
- Provides data via HTTP server (localhost:8080) for web-based overlays
- Exports individual text files for OBS text sources
- Includes a sample HTML overlay template

## Memory Addresses

The mod reads the following data from the game:
- **Win Counts**: EfzRevival.dll+A02CC with offsets 0x4C8 (P1) and 0x4CC (P2)
- **Nicknames**: EfzRevival.dll+A02CC with offsets 0x3BE (P1) and 0x43E (P2) 
- **Characters**: [[efz.exe + 0x390104] + 0x94] (P1) and [[efz.exe + 0x390108] + 0x94] (P2)

## Building

Requirements:
- Visual Studio 2019 or later
- CMake 3.10+
- Detours library (place in 3rdparty/detours/)

```bash
mkdir build
cd build
cmake -A Win32 ..
cmake --build . --config Release
```

## Usage

1. Inject the built DLL into the EFZ process using your preferred DLL injector
2. The mod will automatically start an HTTP server on port 8080
3. Game data will be written to `overlay_data/` folder next to the game executable
4. Use the provided HTML overlay or access JSON data at `http://localhost:8080`

## OBS Integration

### Method 1: Web Source
Add a Browser Source in OBS pointing to the generated `overlay_data/overlay.html` file.

### Method 2: Text Sources
Add Text Sources in OBS reading from individual files:
- `overlay_data/p1_nickname.txt`
- `overlay_data/p2_nickname.txt`  
- `overlay_data/p1_character.txt`
- `overlay_data/p2_character.txt`
- `overlay_data/p1_wins.txt`
- `overlay_data/p2_wins.txt`

### Method 3: Custom Web Source
Create your own web overlay that fetches JSON data from `http://localhost:8080`

## Data Format

JSON response format:
```json
{
  "player1": {
    "nickname": "Player1",
    "character": "Akane", 
    "characterId": 0,
    "winCount": 2
  },
  "player2": {
    "nickname": "Player2",
    "character": "Sayuri",
    "characterId": 4, 
    "winCount": 1
  },
  "gameActive": true
}
```

## Character IDs

- 0: Akane, 1: Akiko, 2: Ikumi, 3: Misaki, 4: Sayuri, 5: Kanna
- 6: Kaori, 7: Makoto, 8: Minagi, 9: Mio, 10: Mishio, 11: Misuzu
- 12: Mizuka, 13: Nagamori, 14: Nanase, 15: ExNanase, 16: Nayuki, 17: NayukiB  
- 18: Shiori, 19: Ayu, 20: Mai, 21: Mayu, 22: MizukaB, 23: Kano
