#include "EditorAssetsManager.h"
#include "nuklear.h"
#include "Logger.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "Editor.h"

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

                if (depth < 10) { // Limit recursion depth
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
            // Calculate indentation based on depth
            float indentWidth = node.depth * 0.02f; // Adjust this value to control indentation amount
            float buttonWidth = 0.95f - indentWidth;
            
            // Use a single row layout to avoid spacing issues
            nk_layout_row_dynamic(ctx, 22, 1);
            
            // Get the full row bounds for custom drawing
            struct nk_rect row_bounds = nk_widget_bounds(ctx);
            
            // Get button bounds for custom drawing
            struct nk_rect button_bounds = row_bounds;
            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            
            // Check for click on the entire row
            bool clicked = nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, button_bounds);
            bool hovering = nk_input_is_mouse_hovering_rect(&ctx->input, button_bounds);
            
            // Calculate positions based on indentation
            float arrow_area_width = 20.0f;
            float arrow_x = button_bounds.x + (node.depth * 20.0f);
            float icon_x = arrow_x + arrow_area_width;
            float text_x = icon_x + 22.0f; // icon width + spacing
            
            // Check if click was on arrow area (only if has children)
            bool arrow_clicked = false;
            if (!node.children.empty()) {
                struct nk_rect arrow_bounds = nk_rect(arrow_x, button_bounds.y, arrow_area_width, button_bounds.h);
                arrow_clicked = nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, arrow_bounds);
                
                if (arrow_clicked) {
                    node.isExpanded = !node.isExpanded;
                    UpdateTreeNodeExpansion(directoryTree, node.fullPath, node.isExpanded);
                }
            }
            
            // Only handle folder click if it wasn't on the arrow
            if (clicked && !arrow_clicked) {
                selectedDirectory = node.fullPath;
                UpdateTreeNodeSelection(directoryTree, selectedDirectory);
                LoadCurrentDirectoryFiles();
                // Unselect any selected file when changing directories
                fileSelected = false;
                showFileInspector = false;
                selectedFile.clear();
                Logger::Info("Directory selected: " + node.fullPath.string());
            }
            
            // Draw background based on state
            struct nk_color bg_color = nk_rgb(255, 255, 255); // Default background
            struct nk_color text_color = nk_rgb(160, 160, 160); // Light gray text color
            
            if (node.isSelected) {
                bg_color = nk_rgb(100, 140, 230);
                text_color = nk_rgb(255, 255, 255);
                nk_fill_rect(canvas, button_bounds, 4.0f, bg_color);
                nk_stroke_rect(canvas, button_bounds, 4.0f, 1.0f, nk_rgb(255, 255, 255));
            } else if (hovering) {
                bg_color = nk_rgb(240, 240, 240);
                text_color = nk_rgb(120, 120, 120); // Darker light gray on hover
                nk_fill_rect(canvas, button_bounds, 4.0f, bg_color);
            }
            
            // Draw expansion arrow if has children
            if (!node.children.empty()) {
                struct nk_color arrow_color = nk_rgb(200, 200, 200);
                float center_x = arrow_x + arrow_area_width * 0.5f;
                float center_y = button_bounds.y + button_bounds.h * 0.5f;
                float arrow_size = 4.0f;
                
                if (node.isExpanded) {
                    // Down arrow (triangle pointing down)
                    float points[6] = {
                        center_x - arrow_size, center_y - arrow_size * 0.5f,  // Top left
                        center_x + arrow_size, center_y - arrow_size * 0.5f,  // Top right
                        center_x, center_y + arrow_size * 0.5f                // Bottom center
                    };
                    nk_fill_polygon(canvas, points, 3, arrow_color);
                } else {
                    // Right arrow (triangle pointing right)
                    float points[6] = {
                        center_x - arrow_size * 0.5f, center_y - arrow_size,  // Top left
                        center_x - arrow_size * 0.5f, center_y + arrow_size,  // Bottom left
                        center_x + arrow_size * 0.5f, center_y                // Right center
                    };
                    nk_fill_polygon(canvas, points, 3, arrow_color);
                }
            }
            
            // Draw custom folder icon with improved colors
            struct nk_color icon_color, icon_outline_color;
            
            if (node.isSelected) {
                icon_color = nk_rgb(255, 215, 0); // Gold color for selected folders
                icon_outline_color = nk_rgb(255, 255, 255);
            } else if (hovering) {
                icon_color = nk_rgb(255, 165, 0); // Orange color on hover
                icon_outline_color = nk_rgb(200, 120, 0);
            } else {
                icon_color = nk_rgb(255, 193, 7); // Amber color for normal state
                icon_outline_color = nk_rgb(180, 140, 0);
            }
            
            float icon_size = 16.0f;
            float icon_y = button_bounds.y + (button_bounds.h - icon_size) * 0.5f;
            
            // Draw folder shape (rectangle with small tab on top)
            struct nk_rect folder_body = nk_rect(icon_x, icon_y + 3, icon_size, icon_size - 3);
            struct nk_rect folder_tab = nk_rect(icon_x, icon_y, icon_size * 0.6f, 4);
            
            nk_fill_rect(canvas, folder_body, 1.0f, icon_color);
            nk_fill_rect(canvas, folder_tab, 1.0f, icon_color);
            nk_stroke_rect(canvas, folder_body, 1.0f, 1.0f, icon_outline_color);
            
            // Draw text next to icon
            struct nk_rect text_rect = nk_rect(text_x, button_bounds.y + 6.0f, 
                                            button_bounds.w - (text_x - button_bounds.x), 
                                            button_bounds.h);
            
            // Use the improved text color
            nk_draw_text(canvas, text_rect, node.name.c_str(), (int)node.name.length(),
                        ctx->style.font, bg_color, text_color);
            
            // Draw children if expanded - fix spacing issue by ensuring proper layout
            if (node.isExpanded && !node.children.empty()) {
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
void EditorPanels::AssetsManager::DrawFileInspector(Runtime::EditorRuntime* editorRuntime) {
    if (!showFileInspector || !fileSelected || !std::filesystem::exists(selectedFile)) {
        return;
    }

    // Handle window resize
    if (editorRuntime->windowResized) {
    // Only resize if we have a valid current context

    Logger::Info("Recieved.");
        if (editorRuntime->windowResized) {
        Logger::Info("Window resized - repositioning inspector");
        
        auto ViewFrameInfo = editorRuntime->GetViewFrameInfo();
        
        if (ViewFrameInfo.isVisible && ViewFrameInfo.width > 0 && ViewFrameInfo.height > 0) {
            float inspectorWidth = inspectorRect.w > 0 ? inspectorRect.w : 400;
            float inspectorHeight = inspectorRect.h > 0 ? inspectorRect.h : 350;
            
            struct nk_rect newRect;
            newRect.x = ViewFrameInfo.x + (ViewFrameInfo.width - inspectorWidth) * 0.5f;
            newRect.y = ViewFrameInfo.y + (ViewFrameInfo.height - inspectorHeight) * 0.5f;
            newRect.w = inspectorWidth;
            newRect.h = inspectorHeight;
            
            // Bounds checking...
            if (newRect.x < ViewFrameInfo.x) newRect.x = ViewFrameInfo.x;
            if (newRect.y < ViewFrameInfo.y) newRect.y = ViewFrameInfo.y;
            if (newRect.x + newRect.w > ViewFrameInfo.x + ViewFrameInfo.width) {
                newRect.x = ViewFrameInfo.x + ViewFrameInfo.width - newRect.w;
            }
            if (newRect.y + newRect.h > ViewFrameInfo.y + ViewFrameInfo.height) {
                newRect.y = ViewFrameInfo.y + ViewFrameInfo.height - newRect.h;
            }
            
            // Force the window to the new position
            nk_window_set_bounds(ctx, "File Inspector", newRect);
            inspectorRect = newRect;  // Update our tracking rect too
        }
        
        inspectorDragging = false;
    }
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

    // Force window to be on top by using NK_WINDOW_BACKGROUND flag and ensuring it's drawn last
    nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE | NK_WINDOW_BACKGROUND;
    
    // Push window to front in Z-order
    nk_window_set_focus(ctx, "File Inspector");
    
    if (nk_begin(ctx, "File Inspector", inspectorRect, window_flags)) {
        
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

    // Update the function signature to include EditorRuntime parameter
    void DrawAssetBrowser(Runtime::EditorRuntime* editorRuntime, int drag_x, int drag_y, int win_h) {
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
            if (nk_group_begin(ctx, "DirectoryTree", NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE)) {
                nk_layout_row_dynamic(ctx, 22, 1);
                nk_label(ctx, "Folders", NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 2, 1);
                nk_spacing(ctx, 1);

                // Root directory - styled to match other folders
                nk_layout_row_dynamic(ctx, 22, 1);
                
                // Get the full row bounds for custom drawing
                struct nk_rect root_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                
                // Check for click and hover
                bool root_clicked = nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, root_bounds);
                bool root_hovering = nk_input_is_mouse_hovering_rect(&ctx->input, root_bounds);
                bool root_selected = (selectedDirectory == currentDirectory);
                
                // Draw background based on state
                struct nk_color root_bg_color = nk_rgb(255, 255, 255);
                struct nk_color root_text_color = nk_rgb(160, 160, 160);
                
                if (root_selected) {
                    root_bg_color = nk_rgb(100, 140, 230);
                    root_text_color = nk_rgb(255, 255, 255);
                    nk_fill_rect(canvas, root_bounds, 4.0f, root_bg_color);
                    nk_stroke_rect(canvas, root_bounds, 4.0f, 1.0f, nk_rgb(255, 255, 255));
                } else if (root_hovering) {
                    root_bg_color = nk_rgb(240, 240, 240);
                    root_text_color = nk_rgb(120, 120, 120);
                    nk_fill_rect(canvas, root_bounds, 4.0f, root_bg_color);
                }
                
                // Draw root folder icon
                struct nk_color root_icon_color, root_icon_outline_color;
                
                if (root_selected) {
                    root_icon_color = nk_rgb(255, 215, 0); // Gold color for selected
                    root_icon_outline_color = nk_rgb(255, 255, 255);
                } else if (root_hovering) {
                    root_icon_color = nk_rgb(255, 165, 0); // Orange color on hover
                    root_icon_outline_color = nk_rgb(200, 120, 0);
                } else {
                    root_icon_color = nk_rgb(255, 193, 7); // Amber color for normal state
                    root_icon_outline_color = nk_rgb(180, 140, 0);
                }
                
                float root_icon_size = 16.0f;
                float root_icon_x = root_bounds.x + 4.0f;
                float root_icon_y = root_bounds.y + (root_bounds.h - root_icon_size) * 0.5f;
                
                // Draw root folder shape (rectangle with small tab on top)
                struct nk_rect root_folder_body = nk_rect(root_icon_x, root_icon_y + 3, root_icon_size, root_icon_size - 3);
                struct nk_rect root_folder_tab = nk_rect(root_icon_x, root_icon_y, root_icon_size * 0.6f, 4);
                
                nk_fill_rect(canvas, root_folder_body, 1.0f, root_icon_color);
                nk_fill_rect(canvas, root_folder_tab, 1.0f, root_icon_color);
                nk_stroke_rect(canvas, root_folder_body, 1.0f, 1.0f, root_icon_outline_color);
                
                // Draw root folder text
                std::string rootName = std::filesystem::path(projectFolderPath).filename().string();
                float root_text_x = root_icon_x + root_icon_size + 6.0f;
                struct nk_rect root_text_rect = nk_rect(root_text_x, root_bounds.y + 6.0f, 
                                                        root_bounds.w - (root_text_x - root_bounds.x), 
                                                        root_bounds.h);
                
                nk_draw_text(canvas, root_text_rect, rootName.c_str(), (int)rootName.length(),
                            ctx->style.font, root_bg_color, root_text_color);
                
                // Handle root folder click
                if (root_clicked) {
                    selectedDirectory = currentDirectory;
                    UpdateTreeNodeSelection(directoryTree, selectedDirectory);
                    LoadCurrentDirectoryFiles();
                    // Unselect any selected file when changing directories
                    fileSelected = false;
                    showFileInspector = false;
                    selectedFile.clear();
                }

                // Draw tree nodes
                for (auto& node : directoryTree) {
                    DrawTreeNode(node);
                }
                
                nk_group_end(ctx);
            }
            
            // Right Panel: Files in Selected Directory as Square Grid
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
                    // Calculate grid layout
                    int squareSize = 80;  // Size of each square
                    int spacing = 8;      // Spacing between squares
                    int panelWidth = (int)(drag_x * 0.6f) - 20; // Available width for grid
                    int itemsPerRow = (panelWidth - spacing) / (squareSize + spacing);
                    if (itemsPerRow < 1) itemsPerRow = 1; // Ensure at least 1 item per row
                    
                    // Iterate through files and create grid layout
                    for (size_t fileIndex = 0; fileIndex < currentDirectoryFiles.size(); fileIndex++) {
                        const auto& file = currentDirectoryFiles[fileIndex];
                        
                        // Start new row if needed
                        if (fileIndex % itemsPerRow == 0) {
                            nk_layout_row_begin(ctx, NK_STATIC, squareSize + 20, itemsPerRow);
                        }
                        
                        nk_layout_row_push(ctx, squareSize + spacing);
                        
                        // Create a group for each file item
                        std::string itemId = "item_" + std::to_string(fileIndex);
                        if (nk_group_begin(ctx, itemId.c_str(), NK_WINDOW_NO_SCROLLBAR)) {
                            
                            // Square button for the file/folder
                            nk_layout_row_static(ctx, squareSize, squareSize, 1);
                            
                            // Get appropriate icon (ONLY the icon, no additional text)
                            std::string icon = GetFileIcon(file.fullPath, file.isDirectory);
                            
                            // Determine button colors based on selection and file type
                            struct nk_color buttonColor, hoverColor;
                            if (fileSelected && selectedFile == file.fullPath) {
                                buttonColor = nk_rgb(100, 150, 200);
                                hoverColor = nk_rgb(120, 170, 220);
                            } else {
                                // Different colors for different file types
                                if (file.isDirectory) {
                                    buttonColor = nk_rgb(70, 90, 120);
                                } else if (IsImageFile(file.fullPath)) {
                                    buttonColor = nk_rgb(80, 120, 80);
                                } else if (IsAudioFile(file.fullPath)) {
                                    buttonColor = nk_rgb(120, 80, 120);
                                } else if (IsVideoFile(file.fullPath)) {
                                    buttonColor = nk_rgb(120, 100, 80);
                                } else {
                                    buttonColor = nk_rgb(90, 90, 90);
                                }
                                hoverColor = nk_rgb(110, 110, 110);
                            }
                            
                            // Push temporary button styling (this is the proper way in Nuklear)
                            nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(buttonColor));
                            nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(hoverColor));
                            
                            // Create the square button with ONLY the icon
                            bool buttonClicked = nk_button_label(ctx, icon.c_str());
                            
                            // Pop the temporary styling immediately after the button is rendered
                            // This ensures it gets popped even if we jump out of the loop early
                            nk_style_pop_style_item(ctx);
                            nk_style_pop_style_item(ctx);
                            
                            if (buttonClicked) {
                                if (file.isDirectory) {
                                    // Add crash prevention for directory navigation
                                    try {
                                        // Additional safety check - ensure currentDirectoryFiles hasn't been modified
                                        if (fileIndex >= currentDirectoryFiles.size()) {
                                            Logger::Error("File index became invalid during directory change");
                                            break;
                                        }
                                        
                                        // Check if directory exists and is accessible
                                        if (std::filesystem::exists(file.fullPath) && 
                                            std::filesystem::is_directory(file.fullPath)) {
                                            
                                            selectedDirectory = file.fullPath;
                                            UpdateTreeNodeSelection(directoryTree, selectedDirectory);
                                            LoadCurrentDirectoryFiles();
                                            // Unselect any selected file when changing directories
                                            fileSelected = false;
                                            showFileInspector = false;
                                            selectedFile.clear();
                                            
                                            Logger::Info("Directory changed to: " + file.fullPath.string());
                                            
                                            // Break out of the loops since currentDirectoryFiles has been updated
                                            goto directory_changed;
                                        } else {
                                            Logger::Error("Directory does not exist or is not accessible: " + file.fullPath.string());
                                        }
                                    } catch (const std::filesystem::filesystem_error& e) {
                                        Logger::Error("Filesystem error when accessing directory: " + std::string(e.what()));
                                    } catch (const std::exception& e) {
                                        Logger::Error("Error accessing directory: " + std::string(e.what()));
                                    }
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
                                        
                                        // Position inspector centered on the ViewFrame
                                        auto ViewFrameInfo = editorRuntime->GetViewFrameInfo(); // Now editorRuntime is available!
                                        
                                        if (ViewFrameInfo.isVisible && ViewFrameInfo.width > 0 && ViewFrameInfo.height > 0) {
                                            // Center the inspector in the view frame
                                            float inspectorWidth = 400;
                                            float inspectorHeight = 350;
                                            
                                            inspectorRect.x = ViewFrameInfo.x + (ViewFrameInfo.width - inspectorWidth) * 0.5f;
                                            inspectorRect.y = ViewFrameInfo.y + (ViewFrameInfo.height - inspectorHeight) * 0.5f;
                                            inspectorRect.w = inspectorWidth;
                                            inspectorRect.h = inspectorHeight;
                                            
                                            // Ensure the inspector stays within the view frame bounds
                                            if (inspectorRect.x < ViewFrameInfo.x) {
                                                inspectorRect.x = ViewFrameInfo.x;
                                            }
                                            if (inspectorRect.y < ViewFrameInfo.y) {
                                                inspectorRect.y = ViewFrameInfo.y;
                                            }
                                            if (inspectorRect.x + inspectorRect.w > ViewFrameInfo.x + ViewFrameInfo.width) {
                                                inspectorRect.x = ViewFrameInfo.x + ViewFrameInfo.width - inspectorRect.w;
                                            }
                                            if (inspectorRect.y + inspectorRect.h > ViewFrameInfo.y + ViewFrameInfo.height) {
                                                inspectorRect.y = ViewFrameInfo.y + ViewFrameInfo.height - inspectorRect.h;
                                            }
                                        } else {
                                            // Fallback to screen center if view frame is not available
                                            float screenWidth = 1920.0f;  // You might want to get actual screen dimensions
                                            float screenHeight = 1080.0f; // or window dimensions instead
                                            inspectorRect.x = (screenWidth * 0.2f);
                                            inspectorRect.y = (screenHeight * 0.15f);
                                            inspectorRect.w = 400;
                                            inspectorRect.h = 350;
                                        }
                                        
                                        inspectorDragging = false;
                                        
                                        Logger::Info("File selected: " + file.fullPath.string());
                                    }
                                }
                            }
                            
                            // File name below the square (truncated if too long)
                            nk_layout_row_dynamic(ctx, 15, 1);
                            std::string displayName = file.name;
                            if (displayName.length() > 12) {
                                displayName = displayName.substr(0, 9) + "...";
                            }
                            nk_label(ctx, displayName.c_str(), NK_TEXT_CENTERED);
                            
                            nk_group_end(ctx);
                        }
                        
                        // End row if it's complete or if it's the last item
                        if ((fileIndex + 1) % itemsPerRow == 0 || fileIndex == currentDirectoryFiles.size() - 1) {
                            nk_layout_row_end(ctx);
                            
                            // Add some vertical spacing between rows
                            nk_layout_row_dynamic(ctx, 5, 1);
                            nk_spacing(ctx, 1);
                        }
                    }
                }
                
                // Label for directory change exit point
                directory_changed:
                
                nk_group_end(ctx);
            }
        }
    }
    nk_end(ctx);
    
    // Draw file inspector popup if needed
    DrawFileInspector(editorRuntime);
    
    ctx->style.window.padding = old_padding;
    ctx->style.window.spacing = old_spacing;
}
}
}