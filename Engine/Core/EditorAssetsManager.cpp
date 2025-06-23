#include "EditorAssetsManager.h"
#include "nuklear.h"
#include "Logger.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#endif

extern struct nk_context *ctx;

namespace EditorPanels {
namespace AssetsManager {

    // Static variables
    static std::string projectFolderPath = "";
    static std::filesystem::path currentDirectory;
    static std::filesystem::path selectedDirectory;
    static bool needsRefresh = true;
    static bool showFileInspector = false;
    static std::filesystem::path selectedFile;
    static bool fileSelected = false;
    
    // File inspector popup state
    static struct nk_rect inspectorRect = {0, 0, 400, 300};
    static bool inspectorDragging = false;
    static struct nk_vec2 inspectorDragOffset = {0, 0};

    // Tree node structure
    struct TreeNode {
        std::string name;
        std::filesystem::path fullPath;
        bool isExpanded;
        bool isSelected;
        int depth;
        std::vector<TreeNode> children;
        
        TreeNode() : isExpanded(false), isSelected(false), depth(0) {}
    };

    // File item structure for right panel
    struct FileItem {
        std::string name;
        std::filesystem::path fullPath;
        bool isDirectory;
        uintmax_t fileSize;
        std::filesystem::file_time_type lastModified;
        std::string fileExtension;
    };

    static std::vector<TreeNode> directoryTree;
    static std::vector<FileItem> currentDirectoryFiles;

    // Utility function to copy text to clipboard
    void CopyToClipboard(const std::string& text) {
#ifdef _WIN32
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            
            size_t size = text.size() + 1;
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) {
                char* pMem = static_cast<char*>(GlobalLock(hMem));
                if (pMem) {
                    strcpy_s(pMem, size, text.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
            }
            CloseClipboard();
            Logger::Info("Copied to clipboard: " + text);
        } else {
            Logger::Error("Failed to open clipboard");
        }
#elif defined(__APPLE__)
        // macOS clipboard implementation
        CFStringRef cfString = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
        if (cfString) {
            PasteboardRef pasteboard;
            if (PasteboardCreate(kPasteboardClipboard, &pasteboard) == noErr) {
                PasteboardClear(pasteboard);
                PasteboardPutItemFlavor(pasteboard, (PasteboardItemID)1, CFSTR("public.utf8-plain-text"), cfString, 0);
                CFRelease(pasteboard);
                Logger::Info("Copied to clipboard: " + text);
            }
            CFRelease(cfString);
        }
#elif defined(__linux__)
        // Linux clipboard implementation using xclip
        std::string command = "echo '" + text + "' | xclip -selection clipboard";
        if (system(command.c_str()) == 0) {
            Logger::Info("Copied to clipboard: " + text);
        } else {
            Logger::Error("Failed to copy to clipboard (xclip not available?)");
        }
#else
        Logger::Warning("Clipboard functionality not implemented for this platform");
#endif
    }

    // Utility function to open file explorer
    void OpenInExplorer(const std::filesystem::path& path) {
#ifdef _WIN32
        std::string command = "explorer /select,\"" + path.string() + "\"";
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        if (CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            Logger::Info("Opened in explorer: " + path.string());
        } else {
            // Fallback: just open the parent directory
            std::string fallbackCommand = "explorer \"" + path.parent_path().string() + "\"";
            if (CreateProcessA(nullptr, const_cast<char*>(fallbackCommand.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                Logger::Info("Opened parent directory in explorer: " + path.parent_path().string());
            } else {
                Logger::Error("Failed to open in explorer: " + path.string());
            }
        }
#elif defined(__APPLE__)
        std::string command = "open -R \"" + path.string() + "\"";
        if (system(command.c_str()) == 0) {
            Logger::Info("Opened in Finder: " + path.string());
        } else {
            Logger::Error("Failed to open in Finder: " + path.string());
        }
#elif defined(__linux__)
        // Try different file managers commonly available on Linux
        std::vector<std::string> fileManagers = {"nautilus", "dolphin", "thunar", "pcmanfm", "nemo"};
        bool success = false;
        
        for (const auto& fm : fileManagers) {
            std::string command = "which " + fm + " > /dev/null 2>&1";
            if (system(command.c_str()) == 0) {
                // File manager found, try to open the file
                command = fm + " \"" + path.parent_path().string() + "\" &";
                if (system(command.c_str()) == 0) {
                    Logger::Info("Opened in " + fm + ": " + path.parent_path().string());
                    success = true;
                    break;
                }
            }
        }
        
        if (!success) {
            Logger::Error("No suitable file manager found to open: " + path.string());
        }
#else
        Logger::Warning("Open in explorer functionality not implemented for this platform");
#endif
    }

    // File type checking functions
    bool IsImageFile(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || 
               ext == ".tga" || ext == ".gif" || ext == ".tiff" || ext == ".webp";
    }

    bool IsAudioFile(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || 
               ext == ".aac" || ext == ".m4a";
    }

    bool IsVideoFile(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".mkv" || 
               ext == ".webm" || ext == ".wmv";
    }

    bool IsTextFile(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".txt" || ext == ".cpp" || ext == ".h" || ext == ".hpp" || 
               ext == ".c" || ext == ".cs" || ext == ".js" || ext == ".html" || 
               ext == ".css" || ext == ".xml" || ext == ".json" || ext == ".md";
    }

    std::string GetFileIcon(const std::filesystem::path& path, bool isDirectory) {
        if (isDirectory) return "[DIR]";
        if (IsImageFile(path)) return "[IMG]";
        if (IsAudioFile(path)) return "[AUD]";
        if (IsVideoFile(path)) return "[VID]";  
        if (IsTextFile(path)) return "[TXT]";
        return "[FILE]";
    }

    // Function to check if a file should be shown
    bool ShouldShowFile(const std::filesystem::path& path) {
        std::string filename = path.filename().string();
        if (filename.empty() || filename[0] == '.') return false;
        if (filename == "Thumbs.db" || filename == "desktop.ini") return false;
        return true;
    }

    // Build directory tree recursively
    void BuildDirectoryTree(const std::filesystem::path& dirPath, std::vector<TreeNode>& nodes, int depth = 0) {
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath) || depth > 10) {
            return;
        }

        try {
            std::vector<std::filesystem::directory_entry> entries;
            
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_directory() && ShouldShowFile(entry.path())) {
                    entries.push_back(entry);
                }
            }

            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                return a.path().filename().string() < b.path().filename().string();
            });

            for (const auto& entry : entries) {
                TreeNode node;
                node.name = entry.path().filename().string();
                node.fullPath = entry.path();
                node.depth = depth;
                node.isExpanded = false;
                node.isSelected = (entry.path() == selectedDirectory);

                if (depth < 5) { // Limit recursion depth
                    BuildDirectoryTree(entry.path(), node.children, depth + 1);
                }

                nodes.push_back(node);
            }
        }
        catch (const std::filesystem::filesystem_error& ex) {
            Logger::Error("Error reading directory tree: " + std::string(ex.what()));
        }
    }

    // Load files in current directory
    void LoadCurrentDirectoryFiles() {
        currentDirectoryFiles.clear();
        
        if (!std::filesystem::exists(selectedDirectory) || !std::filesystem::is_directory(selectedDirectory)) {
            return;
        }

        try {
            std::vector<std::filesystem::directory_entry> entries;
            
            for (const auto& entry : std::filesystem::directory_iterator(selectedDirectory)) {
                if (ShouldShowFile(entry.path())) {
                    entries.push_back(entry);
                }
            }

            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory() > b.is_directory();
                }
                return a.path().filename().string() < b.path().filename().string();
            });

            for (const auto& entry : entries) {
                FileItem item;
                item.name = entry.path().filename().string();
                item.fullPath = entry.path();
                item.isDirectory = entry.is_directory();
                item.fileExtension = entry.path().extension().string();
                
                try {
                    if (!entry.is_directory()) {
                        item.fileSize = std::filesystem::file_size(entry.path());
                    } else {
                        item.fileSize = 0;
                    }
                    item.lastModified = std::filesystem::last_write_time(entry.path());
                } catch (...) {
                    item.fileSize = 0;
                }

                currentDirectoryFiles.push_back(item);
            }
        }
        catch (const std::filesystem::filesystem_error& ex) {
            Logger::Error("Error loading directory files: " + std::string(ex.what()));
        }
    }

    // Update tree node expansion state recursively
    void UpdateTreeNodeExpansion(std::vector<TreeNode>& nodes, const std::filesystem::path& targetPath, bool expanded) {
        for (auto& node : nodes) {
            if (node.fullPath == targetPath) {
                node.isExpanded = expanded;
                return;
            }
            UpdateTreeNodeExpansion(node.children, targetPath, expanded);
        }
    }

    // Update tree node selection state recursively  
    void UpdateTreeNodeSelection(std::vector<TreeNode>& nodes, const std::filesystem::path& selectedPath) {
        for (auto& node : nodes) {
            node.isSelected = (node.fullPath == selectedPath);
            UpdateTreeNodeSelection(node.children, selectedPath);
        }
    }

    // Draw tree node recursively
    void DrawTreeNode(TreeNode& node) {
        std::string indent(node.depth * 12, ' ');
        
        nk_layout_row_begin(ctx, NK_DYNAMIC, 22, 2);
        nk_layout_row_push(ctx, 0.05f);
        
        // Expansion toggle
        if (!node.children.empty()) {
            std::string expandIcon = node.isExpanded ? "▼" : "▶";
            if (nk_button_label(ctx, expandIcon.c_str())) {
                node.isExpanded = !node.isExpanded;
                UpdateTreeNodeExpansion(directoryTree, node.fullPath, node.isExpanded);
            }
        } else {
            nk_spacing(ctx, 1);
        }
        
        nk_layout_row_push(ctx, 0.95f);
        
        // Directory button
        std::string displayName = indent + "[DIR] " + node.name;
        struct nk_style_button* buttonStyle = &ctx->style.button;
        struct nk_color originalColor = buttonStyle->normal.data.color;
        
        if (node.isSelected) {
            buttonStyle->normal.data.color = nk_rgb(100, 150, 200);
            buttonStyle->hover.data.color = nk_rgb(120, 170, 220);
        }
        
        if (nk_button_label(ctx, displayName.c_str())) {
            selectedDirectory = node.fullPath;
            UpdateTreeNodeSelection(directoryTree, selectedDirectory);
            LoadCurrentDirectoryFiles();
            // Unselect any selected file when changing directories
            fileSelected = false;
            showFileInspector = false;
            selectedFile.clear();
            Logger::Info("Directory selected: " + node.fullPath.string());
        }
        
        buttonStyle->normal.data.color = originalColor;
        buttonStyle->hover.data.color = originalColor;
        
        nk_layout_row_end(ctx);
        
        // Draw children if expanded
        if (node.isExpanded) {
            for (auto& child : node.children) {
                DrawTreeNode(child);
            }
        }
    }

    // Format file size
    std::string FormatFileSize(uintmax_t bytes) {
        const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
        int suffixIndex = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && suffixIndex < 4) {
            size /= 1024.0;
            suffixIndex++;
        }
        
        char buffer[64];
        if (suffixIndex == 0) {
            snprintf(buffer, sizeof(buffer), "%.0f %s", size, suffixes[suffixIndex]);
        } else {
            snprintf(buffer, sizeof(buffer), "%.1f %s", size, suffixes[suffixIndex]);
        }
        
        return std::string(buffer);
    }

    // Draw file inspector popup
    void DrawFileInspector() {
        if (!showFileInspector || !fileSelected || !std::filesystem::exists(selectedFile)) {
            return;
        }

        // Make inspector draggable
        if (nk_input_is_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, inspectorRect, nk_true)) {
            if (!inspectorDragging) {
                inspectorDragging = true;
                inspectorDragOffset.x = ctx->input.mouse.pos.x - inspectorRect.x;
                inspectorDragOffset.y = ctx->input.mouse.pos.y - inspectorRect.y;
            }
        }

        if (inspectorDragging) {
            if (ctx->input.mouse.buttons[NK_BUTTON_LEFT].down) {
                inspectorRect.x = ctx->input.mouse.pos.x - inspectorDragOffset.x;
                inspectorRect.y = ctx->input.mouse.pos.y - inspectorDragOffset.y;
            } else {
                inspectorDragging = false;
            }
        }

        if (nk_begin(ctx, "File Inspector", inspectorRect, 
                     NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE)) {
            
            std::string filename = selectedFile.filename().string();
            std::string extension = selectedFile.extension().string();
            
            // File info section
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, ("File: " + filename).c_str(), NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 5, 1);
            nk_spacing(ctx, 1);

            // Properties
            nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
            nk_layout_row_push(ctx, 80);
            nk_label(ctx, "Type:", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 200);
            
            std::string fileType = "Unknown";
            if (IsImageFile(selectedFile)) fileType = "Image";
            else if (IsAudioFile(selectedFile)) fileType = "Audio";
            else if (IsVideoFile(selectedFile)) fileType = "Video";
            else if (IsTextFile(selectedFile)) fileType = "Text";
            else if (!extension.empty()) fileType = extension + " File";
            
            nk_label(ctx, fileType.c_str(), NK_TEXT_LEFT);
            nk_layout_row_end(ctx);

            // File size
            try {
                uintmax_t fileSize = std::filesystem::file_size(selectedFile);
                nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
                nk_layout_row_push(ctx, 80);
                nk_label(ctx, "Size:", NK_TEXT_LEFT);
                nk_layout_row_push(ctx, 200);
                nk_label(ctx, FormatFileSize(fileSize).c_str(), NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
            } catch (...) {
                // Handle error silently
            }

            // File path
            nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
            nk_layout_row_push(ctx, 80);
            nk_label(ctx, "Location:", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 200);
            nk_label(ctx, selectedFile.parent_path().string().c_str(), NK_TEXT_LEFT);
            nk_layout_row_end(ctx);

            nk_layout_row_dynamic(ctx, 10, 1);
            nk_spacing(ctx, 1);

            // Image preview for image files
            if (IsImageFile(selectedFile)) {
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "Preview:", NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 120, 1);
                if (nk_group_begin(ctx, "ImagePreview", NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 100, 1);
                    // Note: Actual image loading would require integration with your graphics system
                    // This is a placeholder for where the image would be displayed
                    struct nk_rect imageRect = nk_widget_bounds(ctx);
                    nk_fill_rect(&ctx->current->buffer, imageRect, 0, nk_rgb(60, 60, 60));
                    
                    // Center text in the preview area
                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, "Image Preview", NK_TEXT_CENTERED);
                    nk_label(ctx, "(Integration Required)", NK_TEXT_CENTERED);
                    
                    nk_group_end(ctx);
                }
            }
            
            // Text preview for small text files
            else if (IsTextFile(selectedFile)) {
                try {
                    uintmax_t fileSize = std::filesystem::file_size(selectedFile);
                    if (fileSize < 1024) { // Only preview small files
                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Preview:", NK_TEXT_LEFT);
                        
                        nk_layout_row_dynamic(ctx, 80, 1);
                        if (nk_group_begin(ctx, "TextPreview", NK_WINDOW_BORDER)) {
                            std::ifstream file(selectedFile);
                            if (file.is_open()) {
                                std::string line;
                                int lineCount = 0;
                                while (std::getline(file, line) && lineCount < 4) {
                                    nk_layout_row_dynamic(ctx, 15, 1);
                                    if (line.length() > 50) {
                                        line = line.substr(0, 47) + "...";
                                    }
                                    nk_label(ctx, line.c_str(), NK_TEXT_LEFT);
                                    lineCount++;
                                }
                                file.close();
                            }
                            nk_group_end(ctx);
                        }
                    }
                } catch (...) {
                    // Handle error silently
                }
            }

            // Action buttons
            nk_layout_row_dynamic(ctx, 5, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, 25, 2);
            if (nk_button_label(ctx, "Open in Explorer")) {
                OpenInExplorer(selectedFile);
            }
            
            if (nk_button_label(ctx, "Copy Path")) {
                CopyToClipboard(selectedFile.string());
            }
        } else {
            // Window was closed - unselect the file
            showFileInspector = false;
            fileSelected = false;
            selectedFile.clear();
            Logger::Info("File inspector closed - file unselected");
        }
        nk_end(ctx);
    }

    // Refresh the entire asset browser
    void RefreshAssetBrowser() {
        if (projectFolderPath.empty()) return;

        directoryTree.clear();
        currentDirectoryFiles.clear();
        
        currentDirectory = std::filesystem::path(projectFolderPath);
        selectedDirectory = currentDirectory;
        
        BuildDirectoryTree(currentDirectory, directoryTree);
        LoadCurrentDirectoryFiles();
        needsRefresh = false;
        
        Logger::Info("Asset browser refreshed");
    }

    // Public interface functions
    void SetProjectFolder(const std::string& folderPath) {
        projectFolderPath = folderPath;
        needsRefresh = true;
        Logger::Info("Assets Manager: Project folder set to " + folderPath);
    }

    std::string GetProjectFolder() {
        return projectFolderPath;
    }

    void DrawAssetBrowser(int drag_x, int drag_y, int win_h) {
        // Refresh if needed
        if (needsRefresh && !projectFolderPath.empty()) {
            RefreshAssetBrowser();
        }

        int asset_h = win_h - drag_y;
        struct nk_rect asset_rect = nk_rect(0, (float)drag_y, (float)drag_x, (float)asset_h);
        
        struct nk_vec2 old_padding = ctx->style.window.padding;
        struct nk_vec2 old_spacing = ctx->style.window.spacing;
        ctx->style.window.padding = nk_vec2(8, 8);
        ctx->style.window.spacing = nk_vec2(4, 4);
        
        if (nk_begin(ctx, "Assets", asset_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            
            if (projectFolderPath.empty()) {
                // No project selected
                nk_layout_row_dynamic(ctx, asset_h - 20, 1);
                if (nk_group_begin(ctx, "NoProject", NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 30, 1);
                    nk_spacing(ctx, 1);
                    nk_label(ctx, "No project folder selected.", NK_TEXT_CENTERED);
                    nk_label(ctx, "Please select a project folder to view assets.", NK_TEXT_CENTERED);
                    nk_group_end(ctx);
                }
            } else {
                // Header
                nk_layout_row_dynamic(ctx, 25, 2);
                std::string headerText = "Project: " + std::filesystem::path(projectFolderPath).filename().string();
                nk_label(ctx, headerText.c_str(), NK_TEXT_LEFT);
                
                if (nk_button_label(ctx, "Refresh")) {
                    needsRefresh = true;
                }

                // Three-panel layout: Tree (40%) | Files (60%)
                float panelRatio[2] = {0.4f, 0.6f};
                nk_layout_row(ctx, NK_DYNAMIC, asset_h - 35, 2, panelRatio);
                
                // Left Panel: Directory Tree
                if (nk_group_begin(ctx, "DirectoryTree", NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 22, 1);
                    nk_label(ctx, "Folders", NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, 2, 1);
                    nk_spacing(ctx, 1);

                    // Root directory
                    nk_layout_row_dynamic(ctx, 22, 1);
                    std::string rootName = "[DIR] " + std::filesystem::path(projectFolderPath).filename().string();
                    struct nk_style_button* buttonStyle = &ctx->style.button;
                    struct nk_color originalColor = buttonStyle->normal.data.color;
                    
                    if (selectedDirectory == currentDirectory) {
                        buttonStyle->normal.data.color = nk_rgb(100, 150, 200);
                    }
                    
                    if (nk_button_label(ctx, rootName.c_str())) {
                        selectedDirectory = currentDirectory;
                        UpdateTreeNodeSelection(directoryTree, selectedDirectory);
                        LoadCurrentDirectoryFiles();
                        // Unselect any selected file when changing directories
                        fileSelected = false;
                        showFileInspector = false;
                        selectedFile.clear();
                    }
                    
                    buttonStyle->normal.data.color = originalColor;

                    // Draw tree nodes
                    for (auto& node : directoryTree) {
                        DrawTreeNode(node);
                    }
                    
                    nk_group_end(ctx);
                }
                
                // Right Panel: Files in Selected Directory
                if (nk_group_begin(ctx, "FileList", NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 22, 1);
                    std::string filesHeader = "Files in " + selectedDirectory.filename().string();
                    nk_label(ctx, filesHeader.c_str(), NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, 2, 1);
                    nk_spacing(ctx, 1);

                    if (currentDirectoryFiles.empty()) {
                        nk_layout_row_dynamic(ctx, 30, 1);
                        nk_label(ctx, "This folder is empty", NK_TEXT_CENTERED);
                    } else {
                        // Files list
                        for (const auto& file : currentDirectoryFiles) {
                            nk_layout_row_begin(ctx, NK_DYNAMIC, 24, 3);
                            nk_layout_row_push(ctx, 0.1f);
                            
                            std::string icon = GetFileIcon(file.fullPath, file.isDirectory);
                            nk_label(ctx, icon.c_str(), NK_TEXT_LEFT);
                            
                            nk_layout_row_push(ctx, 0.6f);
                            
                            // Highlight selected file
                            struct nk_style_button* fileButtonStyle = &ctx->style.button;
                            struct nk_color originalFileColor = fileButtonStyle->normal.data.color;
                            
                            if (fileSelected && selectedFile == file.fullPath) {
                                fileButtonStyle->normal.data.color = nk_rgb(100, 150, 200);
                                fileButtonStyle->hover.data.color = nk_rgb(120, 170, 220);
                            }
                            
                            if (nk_button_label(ctx, file.name.c_str())) {
                                if (file.isDirectory) {
                                    selectedDirectory = file.fullPath;
                                    UpdateTreeNodeSelection(directoryTree, selectedDirectory);
                                    LoadCurrentDirectoryFiles();
                                    // Unselect any selected file when changing directories
                                    fileSelected = false;
                                    showFileInspector = false;
                                    selectedFile.clear();
                                } else {
                                    // Toggle file selection
                                    if (fileSelected && selectedFile == file.fullPath) {
                                        // Clicking selected file again unselects it
                                        fileSelected = false;
                                        showFileInspector = false;
                                        selectedFile.clear();
                                    } else {
                                        // Select new file
                                        selectedFile = file.fullPath;
                                        fileSelected = true;
                                        showFileInspector = true;
                                        
                                        // Better positioning: Center the inspector on screen
                                        // Get screen/window dimensions (you might need to adjust these based on your window system)
                                        float screenWidth = 1920.0f;  // Replace with actual screen width
                                        float screenHeight = 1080.0f; // Replace with actual screen height
                                        
                                        // Position inspector in the center-right area, avoiding the asset browser
                                        inspectorRect.x = (screenWidth * 0.2f);  // 60% across the screen
                                        inspectorRect.y = (screenHeight * 0.15f); // 20% down from top
                                        inspectorRect.w = 400;
                                        inspectorRect.h = 350; // Slightly taller for better content display
                                        
                                        // Reset dragging state when opening
                                        inspectorDragging = false;
                                        
                                        Logger::Info("File selected: " + file.fullPath.string());
                                    }
                                }
                            }
                            
                            fileButtonStyle->normal.data.color = originalFileColor;
                            fileButtonStyle->hover.data.color = originalFileColor;
                            
                            nk_layout_row_push(ctx, 0.3f);
                            if (!file.isDirectory) {
                                nk_label(ctx, FormatFileSize(file.fileSize).c_str(), NK_TEXT_RIGHT);
                            } else {
                                nk_label(ctx, "Folder", NK_TEXT_RIGHT);
                            }
                            
                            nk_layout_row_end(ctx);
                        }
                    }
                    
                    nk_group_end(ctx);
                }
            }
        }
        nk_end(ctx);
        
        // Draw file inspector popup if needed
        DrawFileInspector();
        
        ctx->style.window.padding = old_padding;
        ctx->style.window.spacing = old_spacing;
    }

} // namespace AssetsManager  
} // namespace EditorPanels