#pragma once
#include <string>

struct nk_context;

namespace EditorPanels {
namespace AssetsManager {
    
    // Main function to draw the asset browser
    void DrawAssetBrowser(int drag_x, int drag_y, int win_h);
    
    // Function to set the project folder (called from EditorFolderModal)
    void SetProjectFolder(const std::string& folderPath);
    
    // Function to get the current project folder
    std::string GetProjectFolder();
    
} // namespace AssetsManager
} // namespace EditorPanels