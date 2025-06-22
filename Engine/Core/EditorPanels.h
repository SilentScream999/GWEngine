// EditorPanels.h
#pragma once

#include "Core/Editor.h"

// Forward declare Nuklear image struct
struct nk_image;

namespace EditorPanels {

/// Draws the top menu bar with File, Edit, and Run buttons
void DrawTopMenu(int win_w, int menu_height);

/// Renders the main image view panel
void DrawImageView(struct nk_image img, int drag_x, int menu_height, int top_h);

/// Renders the asset browser panel on the left
void DrawAssetBrowser(int drag_x, int drag_y, int win_h);

/// Renders the sidebar panel on the right
void DrawHierarchy(Runtime::EditorRuntime *editor, int side_x, int side_w, int menu_height, int sidebar_h);

/// Renders the properties panel below the sidebar
void DrawProperties(int side_x, int sidebar_h, int menu_height, int side_w, int prop_h);

} // namespace EditorPanels
