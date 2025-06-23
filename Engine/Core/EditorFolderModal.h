#pragma once
#include <string>

// Forward declaration instead of including nuklear.h
struct nk_context;

namespace EditorFolderModal {
    /**
     * Opens a cross-platform folder selection dialog
     * @return Selected folder path, or empty string if cancelled
     */
    std::string OpenFolderDialog();
    
    /**
     * Draws the folder selection overlay modal
     * @param ctx Nuklear context
     * @param win_w Window width
     * @param win_h Window height
     */
    void DrawFolderSelectionOverlay(nk_context* ctx, int win_w, int win_h);
    
    /**
     * Check if the folder overlay should be shown
     * @return true if overlay should be displayed
     */
    bool ShouldShowFolderOverlay();
    
    /**
     * Get the currently selected folder path
     * @return Selected folder path
     */
    std::string GetSelectedFolderPath();
    
    /**
     * Set whether the folder overlay should be shown
     * @param show true to show overlay, false to hide
     */
    void SetShowFolderOverlay(bool show);
}