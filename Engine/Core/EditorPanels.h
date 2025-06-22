#pragma once

#include <vector>
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

    /// Renders the hierarchy/tree panel
    void DrawHierarchy(Runtime::EditorRuntime *editor, int side_x, int side_w, int menu_height, int sidebar_h);

    /// Renders the properties panel below the hierarchy
    void DrawProperties(Runtime::EditorRuntime *editor, int side_x, int sidebar_h, int menu_height, int side_w, int prop_h);

    /// Returns the *single* selected mesh index, or -1 if none.
    /// (This is the raw index into editor->meshes.)
    int GetSelectedMeshIndex();

    /// Returns all mesh indices in the selected subtree,
    /// as a vector of ints (could be empty).
    std::vector<int> GetSelectedMeshSubtreeIndices();

} // namespace EditorPanels
