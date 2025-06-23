// Include nuklear with all necessary definitions first
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"

#include "EditorFolderModal.h"
#include "EditorAssetsManager.h"  // Add this include
#include "Logger.h"
#include <cstring>
#include <iostream>

// For folder selection dialog
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#elif defined(__linux__)
#include <gtk/gtk.h>
#elif defined(__APPLE__)
// macOS implementation would go here
#endif

namespace EditorFolderModal {
    // Static variables for folder selection state
    static bool showFolderOverlay = true;
    static std::string selectedFolderPath = "";

    // Cross-platform folder selection function
    std::string OpenFolderDialog() {
    #ifdef _WIN32
        // Windows implementation using SHBrowseForFolder
        BROWSEINFOA bi = { 0 };
        bi.lpszTitle = "Select Project Folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        
        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl != nullptr) {
            char path[MAX_PATH];
            if (SHGetPathFromIDListA(pidl, path)) {
                std::string result(path);
                CoTaskMemFree(pidl);
                return result;
            }
            CoTaskMemFree(pidl);
        }
        return "";
        
    #elif defined(__linux__)
        // Linux implementation using GTK
        if (!gtk_init_check(nullptr, nullptr)) {
            Logger::Error("Failed to initialize GTK for folder dialog");
            return "";
        }
        
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Select Project Folder",
            nullptr,
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Open", GTK_RESPONSE_ACCEPT,
            nullptr
        );
        
        std::string result = "";
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename) {
                result = std::string(filename);
                g_free(filename);
            }
        }
        
        gtk_widget_destroy(dialog);
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        
        return result;
    #else
        // Fallback for unsupported platforms
        Logger::Error("Folder dialog not implemented for this platform");
        return "";
    #endif
    }

    void DrawFolderSelectionOverlay(nk_context* ctx, int win_w, int win_h) {
        // Create a semi-transparent background
        struct nk_rect bg_rect = nk_rect(0, 0, win_w, win_h);
        if (nk_begin(ctx, "overlay_bg", bg_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
            // Draw semi-transparent background
            struct nk_color bg_color = nk_rgba(0, 0, 0, 180);
            nk_fill_rect(&ctx->current->buffer, bg_rect, 0, bg_color);
        }
        nk_end(ctx);
        
        // Create the modal dialog with adjusted dimensions
        int dialog_w = 450;
        int dialog_h = selectedFolderPath.empty() ? 150 : 250; // Dynamic height based on content
        int dialog_x = (win_w - dialog_w) / 2;
        int dialog_y = (win_h - dialog_h) / 2;
        
        struct nk_rect dialog_rect = nk_rect(dialog_x, dialog_y, dialog_w, dialog_h);
        if (nk_begin(ctx, "Select Project Folder (To Store Your Project)", dialog_rect, 
                     NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
            
            // Main instruction text
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Please select a project folder to continue.", NK_TEXT_CENTERED);
            
            // Small spacing
            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);
            
            // Show selected folder path if available
            if (!selectedFolderPath.empty()) {
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "Selected folder:", NK_TEXT_LEFT);
                
                // Path display with proper height
                nk_layout_row_dynamic(ctx, 60, 1);
                char path_buffer[512];
                strncpy(path_buffer, selectedFolderPath.c_str(), sizeof(path_buffer) - 1);
                path_buffer[sizeof(path_buffer) - 1] = '\0';
                nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY | NK_EDIT_MULTILINE, 
                                             path_buffer, sizeof(path_buffer), nk_filter_default);
                
                // Small spacing after path
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
            }
            
            // Button area with proper spacing
            nk_layout_row_dynamic(ctx, 35, 2);
            
            if (nk_button_label(ctx, "Browse Folder")) {
                std::string folderPath = OpenFolderDialog();
                if (!folderPath.empty()) {
                    selectedFolderPath = folderPath;
                    Logger::Info("Selected folder: " + folderPath);
                    // Output the directory to console
                    std::cout << "Selected Directory: " << folderPath << std::endl;
                }
            }
            
            // Only enable "Continue" if a folder has been selected
            if (!selectedFolderPath.empty()) {
                if (nk_button_label(ctx, "Continue")) {
                    showFolderOverlay = false;
                    Logger::Info("Project folder set to: " + selectedFolderPath);
                    // Output final confirmation to console
                    std::cout << "Project Directory Confirmed: " << selectedFolderPath << std::endl;
                    
                    // Send the selected folder to EditorAssetsManager
                    EditorPanels::AssetsManager::SetProjectFolder(selectedFolderPath);
                }
            } else {
                // Create a disabled-looking button
                struct nk_style_button original_style = ctx->style.button;
                
                ctx->style.button.normal = nk_style_item_color(nk_rgb(100, 100, 100));
                ctx->style.button.hover = ctx->style.button.normal;
                ctx->style.button.active = ctx->style.button.normal;
                ctx->style.button.text_normal = nk_rgb(150, 150, 150);
                ctx->style.button.text_hover = ctx->style.button.text_normal;
                ctx->style.button.text_active = ctx->style.button.text_normal;
                
                nk_button_label(ctx, "Continue");
                
                // Restore original style
                ctx->style.button = original_style;
            }
        }
        nk_end(ctx);
    }

    bool ShouldShowFolderOverlay() {
        return showFolderOverlay;
    }

    std::string GetSelectedFolderPath() {
        return selectedFolderPath;
    }

    void SetShowFolderOverlay(bool show) {
        showFolderOverlay = show;
    }
}