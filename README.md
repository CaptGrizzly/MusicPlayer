# Music Player Project

## Overview
This project implements a music player with **two main components**:  
- `musicPlayer.cpp` – Handles music playback (play, pause, stop, volume control).  
- `remote.cpp` – Manages remote control functionality for the player.  

The configuration and dependencies are specified in the `platformio.ini` file.

## Files
### `musicPlayer.cpp`
- Implements core music playback functionality.
- **Functions:**  
  - `play()`: Start playing music.  
  - `pause()`: Pause playback.  
  - `stop()`: Stop playback.  
  - `setVolume(int level)`: Adjust volume.  

### `remote.cpp`
- Handles remote control functionalities.
- **Functions:**  
  - `initializeRemote()`: Set up the remote control.  
  - `handleRemoteInput()`: Process remote commands.  

### `platformio.ini`
- Configuration file for PlatformIO.
- Specifies **environment settings** and **dependencies**.

## How to Use
### **Prerequisites**
- Install [PlatformIO](https://platformio.org/install).
- Ensure your development environment supports the target hardware.

### **Build & Run Steps**
1. **Clone the Repository**  
   ```sh
   git clone <repository-url>
   cd <repository-directory>
