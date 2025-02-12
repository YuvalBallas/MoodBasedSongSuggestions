# Mood-Based Song Suggestions 🎵

## Overview
Mood-Based Song Suggestions is a C++ application that suggests songs based on the user's selected mood. It fetches song recommendations using the Last.fm API and allows users to mark songs as favorites for future reference.


## Features 
- 🎵 Fetch song recommendations based on mood selection.

- ❤️ Save favorite songs and manage them easily.

- ▶️ Play songs directly from Last.fm.

- 🔀 Shuffle and sort favorite songs.

- ❌ Remove songs from favorites.

- 🖥️ Intuitive GUI using ImGui and DirectX.


## Installation

### Prerequisites

- Windows OS

- Visual Studio (2022 recommended)

- C++ compiler supporting C++17 or later

- Last.fm API key (included in the code, replace with your own if needed)

### Steps
1. **Clone the repository**:
    ```sh
    git clone https://github.com/YuvalBallas/MoodBasedSongSuggestions.git
2. **Open the project**:  
   - Navigate to the project folder.  
   - Open the `.sln` file in **Visual Studio**.

3. **Build the project**:  
   - Select **Debug** or **Release** mode.  
   - Click **Build** > **Build Solution** (or press `Ctrl+Shift+B`).

4. **Run the application**:  
   - Click **Debug** > **Start Without Debugging** (or press `Ctrl+F5`).


## Usage

1. **Select a mood** from the dropdown.

2. Click **Get Songs** to fetch recommendations.

3. Click **Play** to open the song on Last.fm.

4. Click **Favorite** to save the song.

5. Manage your saved songs in the **Favorites** window - play, remove, shuffle, or sort them easily.


## Technologies Used

- C++

- ImGui (for GUI)

- DirectX 11

- Last.fm API

- JSON (nlohmann/json)

- HTTP requests (httplib)

- OpenSSL (for secure communication and encryption)
