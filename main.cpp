#define WIN32_LEAN_AND_MEAN  
#define NOMINMAX  
#define STB_IMAGE_IMPLEMENTATION  

#include <windows.h>  
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "httplib.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <GL/gl.h>  
#include "stb_image.h"
#include <shellapi.h> 
#include <thread>
#include <mutex>
#include <atomic> 
#include <fstream>
#include <filesystem> 
namespace fs = std::filesystem;
#include <algorithm>

using json = nlohmann::json;

// Global Variables
std::mutex dataMutex; // Global mutex to protect songList
std::string FAVORITES_FILE = "favorites.txt";  // File to store favorite songs
std::atomic<int> selectedMood = -1;  // atomic to prevent race conditions
std::vector<std::pair<std::string, std::string>> songList;  // Stores (Song Name, Artist)
std::vector<std::pair<std::string, std::string>> favoriteSongs; // Stores favorites
std::thread fetchThread;
std::atomic<bool> isFetching(false); // Atomic flag to ensure only one fetch operation at a time

// Constants for API
const std::string API_KEY = "eb06d13a5235407d221efb58f6de9361";  
const std::string BASE_URL = "http://ws.audioscrobbler.com/2.0/";

// DirectX Variables
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Mood Selection Variables
std::vector<std::string> moods = { "Happy", "Sad", "Relaxed", "Energetic", "Chill", "Excited" };

// Function Declarations
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Function to Fetch Songs from Last.fm API in a separate thread
void fetchSongsByMood(const std::string& mood) {
    if (isFetching.load()) return;  // Prevent multiple fetches at the same time

    isFetching.store(true);

    // If an old thread is still running, join it before starting a new one
    if (fetchThread.joinable()) {
        fetchThread.join();
    }
    
    // Start the new thread
    fetchThread = std::thread([mood]() {
        std::vector<std::pair<std::string, std::string>> tempSongList; // Temporary storage

        // HTTP client setup
        httplib::Client client("ws.audioscrobbler.com");      
        std::string endpoint = "/2.0/?method=tag.gettoptracks&tag=" + mood + "&api_key=" + API_KEY + "&format=json";
        auto response = client.Get(endpoint.c_str());
        
        if (response && response->status == 200) {
            try {
                json jsonResponse = json::parse(response->body);
                // Iterate over tracks
                for (const auto& track : jsonResponse["tracks"]["track"]) {
                    std::string songName = track["name"];
                    std::string artistName = track["artist"]["name"];
                    // Store song details
                    tempSongList.emplace_back(songName, artistName);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
            }
        }
        else {
            std::cerr << "Failed to fetch data: " << (response ? response->status : -1) << std::endl;
        }

        // Update shared data safely
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            songList = std::move(tempSongList);
        }

        // Reset the atomic flag
        isFetching.store(false);
    });
    
}

// Ensures any active fetch completes before exiting
void cleanupThreads() {
    if (fetchThread.joinable()) {
        fetchThread.join();  
    }
}

// Save Favorite Songs to File
void saveFavorites() {
    std::ofstream file(FAVORITES_FILE);
    if (!file) {
        std::cerr << "Error: Could not open favorites file for writing!" << std::endl;
        return;
    }
    for (const auto& fav : favoriteSongs) {
        file << fav.first << " | " << fav.second << "\n";
    }
    file.close();
}

// Load Favorites from File 
void loadFavorites() {
    if (!fs::exists(FAVORITES_FILE)) return;  
    std::ifstream file(FAVORITES_FILE);
    if (!file) {
        std::cerr << "Error: Could not open favorites file for reading!" << std::endl;
        return;
    }
    favoriteSongs.clear();  
    std::string line;
    while (std::getline(file, line)) {
        size_t separator = line.find(" | ");
        if (separator != std::string::npos) {
            std::string songName = line.substr(0, separator);
            std::string artistName = line.substr(separator + 3);
            favoriteSongs.emplace_back(songName, artistName);
        }
    }
    file.close();
}


// Render ImGui Interface
void RenderImGui() {
    // Set custom style
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Header] = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);  // Selected item (mood dropdown)
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);  // Hover effect
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.9f, 0.4f, 0.4f, 1.0f);  // Active selection
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);  // Window header 
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);  // Active window header
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);  // Collapsed window header
    style.Colors[ImGuiCol_Button] = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);  // Button color
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    style.WindowRounding = 10.0f;  // Rounded window corners
    style.FrameRounding = 10.0f;   // Rounded button corners
    style.FramePadding = ImVec2(8, 8);  // Make title bars taller
    style.WindowPadding = ImVec2(20, 20);  // Add more padding inside windows
   

    // Window Sizes
    float mainWindowWidth = 750;
    float mainWindowHeight = 650;
    float favoritesWindowWidth = 480;
    float favoritesWindowHeight = 650;
    float spacingBetweenWindows = 50;  

    // Main Window
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(mainWindowWidth, mainWindowHeight), ImGuiCond_Always);
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  
    ImGui::Begin("Mood-Based Song Suggestions", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    ImGui::PopFont();  
    ImGui::Spacing();  

    // Mood Selection Dropdown
    ImGui::Text("Select Your Mood:");
    std::string moodDisplayText = (selectedMood.load(std::memory_order_relaxed) == -1) ? "Select Mood" : moods[selectedMood.load(std::memory_order_relaxed)];
    if (ImGui::BeginCombo("##mood", moodDisplayText.c_str())) {
        for (int i = 0; i < moods.size(); i++) {
            bool isSelected = (selectedMood.load(std::memory_order_relaxed) == i);
            if (ImGui::Selectable(moods[i].c_str(), isSelected)) {
                selectedMood.store(i, std::memory_order_relaxed);  // Store new value atomically
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Fetch Songs Button
    if (ImGui::Button("Get Songs", ImVec2(200, 42))) {
        int currentMood = selectedMood.load(std::memory_order_relaxed);  
        if (currentMood != -1) {
            fetchSongsByMood(moods[currentMood]);
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();

    // Display Songs
    for (const auto& song : songList) {
        ImGui::Text("%s - %s", song.first.c_str(), song.second.c_str());

        // Button to Play Song
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
        std::string buttonLabel = "Play##" + song.first;
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(100, 32))) {
            std::string url = "https://www.last.fm/music/" + song.second + "/_/" + song.first;
            ShellExecute(0, 0, url.c_str(), 0, 0, SW_SHOW);
        }
        ImGui::SameLine();

        // Button to Save as Favorite
        std::string favButtonLabel = "Favorite##" + song.first;
        if (ImGui::Button(favButtonLabel.c_str(), ImVec2(100, 32))) {
            bool alreadyFavorite = false;
            for (const auto& fav : favoriteSongs) { // Check if the song is already in favorites
                if (fav.first == song.first && fav.second == song.second) {
                    alreadyFavorite = true;
                    break;
                }
            }
            // Add only if it's not already in favorites
            if (!alreadyFavorite) {
                favoriteSongs.push_back(song);
                saveFavorites();  // Save to file whenever a favorite is added
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
    }
    ImGui::End();

    // Favorites Window 
    ImGui::SetNextWindowPos(ImVec2(30 + mainWindowWidth + spacingBetweenWindows, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(favoritesWindowWidth, favoritesWindowHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);  // Slight transparency
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Begin("Favorites", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::PopFont();

    // Shuffle & Sort Buttons
    if (ImGui::Button("Shuffle Favorites")) { 
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(favoriteSongs.begin(), favoriteSongs.end(), g);
        saveFavorites();
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort Favorites")) { 
        std::sort(favoriteSongs.begin(), favoriteSongs.end());
        saveFavorites();
    }
    ImGui::Spacing();
    ImGui::Separator();

    // Display Favorite Songs
    if (favoriteSongs.empty()) {
        ImGui::Text("No favorites yet. Add the songs you liked!");
    }
    else {
        int removeIndex = -1;  // Store the index of the song to remove
        for (size_t i = 0; i < favoriteSongs.size(); i++) {
            ImGui::BeginGroup();
            std::string songText = favoriteSongs[i].first + " - " + favoriteSongs[i].second;
            if (songText.length() > 40) {
                songText = songText.substr(0, 37) + "...";
                ImGui::Text("%s", songText.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s - %s", favoriteSongs[i].first.c_str(), favoriteSongs[i].second.c_str());
                }
            }
            else {
                ImGui::Text("%s", songText.c_str());
            }
            
            ImGui::SameLine(favoritesWindowWidth - 90);
 
            // Popup Menu for Options
            if (ImGui::Button(("...##favOptions" + std::to_string(i)).c_str(), ImVec2(40, 30))) {
                ImGui::OpenPopup(("FavMenu##" + std::to_string(i)).c_str());
            }
            ImGui::EndGroup();
            
            if (ImGui::BeginPopup(("FavMenu##" + std::to_string(i)).c_str())) {
                if (ImGui::MenuItem("Play")) {
                    std::string url = "https://www.last.fm/music/" + favoriteSongs[i].second + "/_/" + favoriteSongs[i].first;
                    ShellExecute(0, 0, url.c_str(), 0, 0, SW_SHOW);
                }
                if (ImGui::MenuItem("Remove")) {
                    removeIndex = static_cast<int>(i);  // Mark this song for removal
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::Separator();
        }
        // Remove the song after the loop 
        if (removeIndex != -1) {
            favoriteSongs.erase(favoriteSongs.begin() + removeIndex);
            saveFavorites();
        }
    }
    ImGui::End();
}


// DirectX Initialization
void InitDirectX(HWND hwnd) {
    std::cout << "Initializing DirectX..." << std::endl;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pd3dDeviceContext);

    if (FAILED(hr)) {
        std::cerr << "ERROR: DirectX Initialization Failed! HRESULT: " << hr << std::endl;
        exit(-1);
    }
    std::cout << "DirectX initialized successfully!" << std::endl;

    if (!g_pSwapChain) {
        std::cerr << "ERROR: g_pSwapChain is NULL after D3D11CreateDeviceAndSwapChain!" << std::endl;
        exit(-1);
    }

    // Create render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

    if (FAILED(hr) || !pBackBuffer) {
        std::cerr << "ERROR: Failed to get back buffer! HRESULT: " << hr << std::endl;
        exit(-1);
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();

    if (FAILED(hr) || !g_mainRenderTargetView) {
        std::cerr << "ERROR: Failed to create Render Target View! HRESULT: " << hr << std::endl;
        exit(-1);
    }

    std::cout << "DirectX setup complete!" << std::endl;
}


// Win32 Message Handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* pBackBuffer;
            g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


// Main Function
int main() {
    // Create Window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Mood-Based Song Suggestions"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Mood-Based Song Suggestions"), WS_OVERLAPPEDWINDOW, 100, 100, 1400, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize DirectX & ImGui
    InitDirectX(hwnd);
    ImGui::CreateContext();
    loadFavorites(); // Load saved favorited when the app starts
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 22.0f); // Adjust path & size
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Show Window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Main Loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Debugging: Ensure DirectX objects are valid
        if (!g_pSwapChain || !g_pd3dDeviceContext || !g_mainRenderTargetView) {
            std::cerr << "ERROR: One or more DirectX objects are NULL before rendering!" << std::endl;
            exit(-1);
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render UI
        RenderImGui();
        std::cout << "ImGui frame rendered!" << std::endl;

        // Set render target
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        std::cout << "Set render target successful!" << std::endl;

        // Clear screen
        ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        std::cout << "ClearRenderTargetView successful!" << std::endl;

        // Render ImGui
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        std::cout << "ImGui rendering completed!" << std::endl;

        // Present frame (this is where it might crash)
        HRESULT hr = g_pSwapChain->Present(1, 0);
        if (FAILED(hr)) {
            std::cerr << "ERROR: SwapChain->Present() failed! HRESULT: " << hr << std::endl;
            exit(-1);
        }
        std::cout << "Frame presented successfully!" << std::endl;

    }

    // Ensure all threads finish before exiting
    cleanupThreads();

    return 0;
}
