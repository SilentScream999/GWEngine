#pragma once
#include <string>
#include "Editor.h"
struct nk_context;

namespace EditorPanels {
namespace AssetsManager {
   
    // Main function to draw the asset browser
    void DrawAssetBrowser(Runtime::EditorRuntime* editorRuntime, int drag_x, int drag_y, int win_h);
   
    // Function to set the project folder (called from EditorFolderModal)
    void SetProjectFolder(const std::string& folderPath);
   
    // Function to get the current project folder
    std::string GetProjectFolder();

    void DrawFileInspector(Runtime::EditorRuntime* editorRuntime);

    // New functions for inspector positioning (no struct definition needed in header)
    void CenterInspectorInViewFrame(Runtime::EditorRuntime* editorRuntime);
    bool HasViewFrameChanged(Runtime::EditorRuntime* editorRuntime);
    void UpdateInspectorPosition(Runtime::EditorRuntime* editorRuntime);
   
} // namespace AssetsManager
} // namespace EditorPanels