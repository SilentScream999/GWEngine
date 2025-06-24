#include "EditorPanels.h"
#include "EditorAssetsManager.h"
#include "Core/Editor.h"
#include "Renderer/RendererManager.h"
#include "nuklear.h"
#include <SDL.h>
#include <cstdio>
#include <algorithm>

extern struct nk_context *ctx;
extern SDL_Renderer* renderer;

int selected_item = 0;
int end_selection = 0;

extern int selected_item;
extern int end_selection;
extern std::vector<int> entityIndexes;

namespace EditorPanels {

void DrawTopMenu(int win_w, int menu_height) {
	struct nk_rect menu_rect = nk_rect(0, 0, (float)win_w, (float)menu_height);
	if (nk_begin(ctx, "TopMenu", menu_rect, NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_begin(ctx, NK_STATIC, menu_height, 3);
		nk_layout_row_push(ctx, 50.0f);
		nk_button_label(ctx, "File");
		nk_layout_row_push(ctx, 50.0f);
		nk_button_label(ctx, "Edit");
		nk_layout_row_push(ctx, 50.0f);
		nk_button_label(ctx, "Run");
		nk_layout_row_end(ctx);
	}
	nk_end(ctx);
}

void DrawImageView(struct nk_image img, int drag_x, int menu_height, int top_h) {
	struct nk_rect img_rect = nk_rect(0, (float)menu_height, (float)drag_x, (float)top_h);
	if (nk_begin(ctx, "Main Image View", img_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_static(ctx, top_h, drag_x, 1);
		nk_image(ctx, img);
	}
	nk_end(ctx);
}

void DrawAssetBrowser(Runtime::EditorRuntime* editorRuntime, int drag_x, int drag_y, int win_h) {
    // Delegate to the AssetsManager with the correct signature
    AssetsManager::DrawAssetBrowser(editorRuntime, drag_x, drag_y, win_h);
}

#include <vector>
#include <string>
#include <cstdio> // for printf
#include <cmath>  // for fabs, sqrt

static std::vector<std::string> entityNames = { "Scene Root", "Camera", "Light", "Mesh01", "SubMesh" };
static std::vector<int> entityLevels = { 0, 1, 1, 1, 2 };
static std::vector<int> entityIndexes = { -1, -1, -1, -1, -1 };

void DrawHierarchy(Runtime::EditorRuntime *editor, int side_x, int side_w, int menu_height, int sidebar_h) {
	struct nk_rect side_rect = nk_rect((float)side_x, (float)menu_height, (float)side_w, (float)sidebar_h);
	static int selectedIndex = -1;
	// Drag state
	static int dragIndex = -1;
	static bool dragging = false;
	static struct nk_vec2 dragStartPos = {0,0};

	// Base reference width to compare against (tweak as needed)
	const float base_width = 300.0f;
	float ui_scale = side_w / base_width;
	if (ui_scale < 0.90f) ui_scale = 0.90f; // Minimum 75% (25% reduction max)
	if (ui_scale > 1.3f) ui_scale = 1.3f; // optional clamp

	// Scale text and font-related measurements
	const float base_font_size = 13.0f; // Adjust this to your default font size
	float scaled_font_height = base_font_size * ui_scale;

	// Get a non-const pointer to the font to modify its height
	struct nk_user_font *font = (struct nk_user_font*)ctx->style.font;
	float original_font_height = font->height;
	
	// Scale the font size
	font->height = scaled_font_height;

	if (nk_begin(ctx, "Hierarchy", side_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		std::vector<struct nk_rect> occupiedRects;

		 // ── Header: "Hierarchy" + "+" + "Delete" as fixed small square buttons with centered symbols ─────
		const float btn_size = 20.0f * ui_scale;
		const float padding = 4.0f * ui_scale;

		// Scale the header height based on font size
		float header_height = std::max(btn_size, scaled_font_height + padding);

		// Begin a static row: 3 columns: label + plus + delete
		nk_layout_row_begin(ctx, NK_STATIC, header_height, 3);

		// 1) Label: calculate width to leave room for both buttons plus more padding
		float label_w = side_w - (btn_size * 2) - (padding * 6); // room for 2 buttons + extra padding
		if (label_w < 40 * ui_scale) label_w = 40 * ui_scale; // scale minimum label width

		nk_layout_row_push(ctx, label_w);
		nk_label(ctx, "Hierarchy", NK_TEXT_LEFT);
		{
			struct nk_rect r = nk_widget_bounds(ctx);
			occupiedRects.push_back(r);
		}

		// 2) Create button: fixed square with centered plus symbol
		nk_layout_row_push(ctx, btn_size);

		// Method 1: Force text alignment to center for the button
		struct nk_style_button original_plus_style = ctx->style.button;
		ctx->style.button.text_alignment = NK_TEXT_CENTERED; // Ensure horizontal centering

		bool plusClicked = nk_button_symbol(ctx, NK_SYMBOL_PLUS);

		// Restore original style
		ctx->style.button = original_plus_style;

		{
			struct nk_rect r = nk_widget_bounds(ctx);
			occupiedRects.push_back(r);
		}

		if (plusClicked) {
			int parentLevel = 0;
			int insertIndex = (int)entityNames.size();
			
			if (selectedIndex >= 0 && selectedIndex < (int)entityNames.size()) {
				parentLevel = entityLevels[selectedIndex];
				insertIndex = selectedIndex + 1;
				while (insertIndex < (int)entityNames.size() && entityLevels[insertIndex] > parentLevel) {
					insertIndex++;
				}
			}
			
			entityNames.insert(entityNames.begin() + insertIndex, "NewEntity");
			entityLevels.insert(entityLevels.begin() + insertIndex, parentLevel + 1);
			selectedIndex = insertIndex;
			
			// here we handle adding new entities to the meshlist
			int meshindex = editor->AddMesh("assets/models/basicplane.obj");
			entityIndexes.insert(entityIndexes.begin() + insertIndex, meshindex);
			printf("Added NewEntity under parent level %d at index %d with true index %d\n", parentLevel, insertIndex, meshindex);
		}

		// 3) Delete button: fixed square, only active if selectedIndex > 0
		nk_layout_row_push(ctx, btn_size);
		bool deleteClicked = false;
		bool canDelete = (selectedIndex > 0 && selectedIndex < (int)entityNames.size());

		// Always render the same type of button to maintain consistent positioning
		if (canDelete) {
			// Store original style and ensure centering
			struct nk_style_button original_delete_style = ctx->style.button;
			ctx->style.button.text_alignment = NK_TEXT_CENTERED; // Ensure horizontal centering
			
			deleteClicked = nk_button_symbol(ctx, NK_SYMBOL_MINUS);
			
			// Restore original style
			ctx->style.button = original_delete_style;
		} else {
			// Use the same button call but store the original style first
			struct nk_style_button original_style = ctx->style.button;
			
			// Set disabled appearance
			ctx->style.button.normal = ctx->style.button.hover; // Make it look disabled
			ctx->style.button.hover = ctx->style.button.normal;
			ctx->style.button.active = ctx->style.button.normal;
			ctx->style.button.text_normal = nk_rgb(128, 128, 128); // Gray text
			ctx->style.button.text_hover = nk_rgb(128, 128, 128);
			ctx->style.button.text_active = nk_rgb(128, 128, 128);
			ctx->style.button.text_alignment = NK_TEXT_CENTERED; // Ensure horizontal centering even when disabled
			
			nk_button_symbol(ctx, NK_SYMBOL_MINUS); // Same call, just styled differently
			
			// Restore original style
			ctx->style.button = original_style;
		}

		{
			struct nk_rect r = nk_widget_bounds(ctx);
			occupiedRects.push_back(r);
		}

		if (deleteClicked && selectedIndex > 0 && selectedIndex < (int)entityNames.size()) {
			int delIndex = selectedIndex;
			int delLevel = entityLevels[delIndex];
			
			// find end of subtree
			int end = delIndex + 1;
			while (end < (int)entityNames.size() && entityLevels[end] > delLevel) {
				end++;
			}
			
			// find parent index
			int parentIndex = -1;
			for (int k = delIndex - 1; k >= 0; --k) {
				if (entityLevels[k] < delLevel) {
					parentIndex = k;
					break;
				}
			}
			// erase subtree [delIndex, end)
			entityNames.erase(entityNames.begin() + delIndex, entityNames.begin() + end);
			entityLevels.erase(entityLevels.begin() + delIndex, entityLevels.begin() + end);
			
			for (int i = delIndex; i < end; i++) {
				bool worked = editor->DeleteMesh(entityIndexes[i]);
				printf("Worked %d for index %d real indx %d\n", worked, i, entityIndexes[i]);
			}
			entityIndexes.erase(entityIndexes.begin() + delIndex, entityIndexes.begin() + end);
			
			// set selection to parent (or -1 if none)
			int oldSel = selectedIndex;
			selectedIndex = parentIndex;
			if (parentIndex >= 0) {
				printf("Deleted subtree at index %d; now selected parent '%s' at index %d\n",
					   delIndex, entityNames[parentIndex].c_str(), parentIndex);
			} else {
				// if somehow parentIndex < 0, just note deletion without selecting
				printf("Deleted subtree at index %d; no parent to select\n", delIndex);
			}
		}

		nk_layout_row_end(ctx);

		// ── Create scrollable region for the entity list ──────────────────────────
		// Scale row height based on font size with some padding
		const float row_h = (scaled_font_height + 8.0f * ui_scale); // font height + padding
		const int count = (int)entityNames.size();
		const float total_content_height = count * row_h + 10.0f * ui_scale; // Scale extra padding
		const float available_height = sidebar_h - (40.0f * ui_scale); // Scale header space

		// Create a group with scrollbar
		nk_layout_row_dynamic(ctx, available_height, 1);
		if (nk_group_begin(ctx, "entity_list", 0)) {

			// Layout parameters for items - all scaled
			const float x_offset = 12.0f * ui_scale;
			const float step = 14.0f * ui_scale;
			const float text_pad = 4.0f * ui_scale;
			struct nk_color line_color = nk_rgb(64, 64, 64);
			struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
			struct nk_rect group_bounds = nk_layout_widget_bounds(ctx);

			std::vector<std::pair<float, std::pair<float, float>>> drawnVerticalLines;

			// Mouse & click state
			bool mouseDown = nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
			bool mouseReleased = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
			struct nk_vec2 mousePos = ctx->input.mouse.pos;

			// Set up the content area with proper height for scrolling
			nk_layout_space_begin(ctx, NK_STATIC, total_content_height, count);

			// Iterate entities
			for (int i = 0; i < count; ++i) {
				int level = entityLevels[i];
				float y_pos = i * row_h;

				// Set layout space for this item
				nk_layout_space_push(ctx, nk_rect(0, y_pos, side_w - 20 * ui_scale, row_h)); // Scale scrollbar space

				// CRITICAL FIX: Get the actual widget bounds which include scroll offset
				struct nk_rect widget_bounds = nk_widget_bounds(ctx);
				float y_center = widget_bounds.y + row_h * 0.5f;

				// Draw ancestor vertical lines with scaled thickness
				float line_thickness = std::max(1.0f, ui_scale);
				for (int l = 0; l < level; ++l) {
					bool needsLine = false;
					for (int j = i + 1; j < count; ++j) {
						if (entityLevels[j] <= l) break;
						needsLine = true;
						break;
					}
					if (needsLine) {
						float vx = group_bounds.x + x_offset + l * step;
						float vy0 = y_center - row_h*0.5f;
						float vy1 = y_center + row_h*0.5f;
						bool drawn = false;
						for (auto &dl : drawnVerticalLines) {
							if (fabs(dl.first - vx) < 0.1f && fabs(dl.second.first - vy0) < 0.1f) {
								drawn = true; break;
							}
						}
						if (!drawn) {
							nk_stroke_line(canvas, vx, vy0, vx, vy1, line_thickness, line_color);
							drawnVerticalLines.push_back({vx, {vy0, vy1}});
						}
					}
				}

				// Connector ├─ or └─ with scaled corner radius
				bool isLast = true;
				for (int j = i + 1; j < count; ++j) {
					if (entityLevels[j] == level) { isLast = false; break; }
					if (entityLevels[j] < level) break;
				}
				if (level > 0) {
					float px = group_bounds.x + x_offset + (level - 1)*step;
					float hx = px + step;
					float vy = y_center;
					const float cr = 3.0f * ui_scale; // Scale corner radius
					if (isLast) {
						nk_stroke_line(canvas, px, vy - row_h*0.5f, px, vy - cr, line_thickness, line_color);
						nk_stroke_line(canvas, px, vy - cr, px + cr, vy, line_thickness, line_color);
						nk_stroke_line(canvas, px + cr, vy, hx, vy, line_thickness, line_color);
					} else {
						nk_stroke_line(canvas, px, vy - row_h*0.5f, px, vy + row_h*0.5f, line_thickness, line_color);
						nk_stroke_line(canvas, px, vy - cr, px + cr, vy, line_thickness, line_color);
						nk_stroke_line(canvas, px + cr, vy, hx, vy, line_thickness, line_color);
					}
				}
				// Root vertical if has children
				if (level == 0) {
					bool hasChild = false;
					for (int j = i + 1; j < count; ++j) {
						if (entityLevels[j] > level) { hasChild = true; break; }
						if (entityLevels[j] <= level) break;
					}
					if (hasChild) {
						float vx = group_bounds.x + x_offset + level*step;
						nk_stroke_line(canvas, vx, y_center, vx, y_center + row_h*0.5f, line_thickness, line_color);
					}
				}

				// Create a widget for this entity - moved up to get proper bounds
				struct nk_rect label_rect;
				label_rect.x = group_bounds.x + x_offset + level * step + text_pad;
				label_rect.y = widget_bounds.y;
				label_rect.w = side_w - (label_rect.x - group_bounds.x + 20.0f * ui_scale); // Scale scrollbar account
				label_rect.h = row_h;

				// Scale selection highlight border radius and thickness
				float highlight_radius = 4.0f * ui_scale;
				float highlight_thickness = std::max(1.0f, ui_scale);

				// Highlight if selected
				if (i == selectedIndex) {
					nk_fill_rect(canvas, label_rect, highlight_radius, nk_rgb(100, 140, 230));
					nk_stroke_rect(canvas, label_rect, highlight_radius, highlight_thickness, nk_rgb(255, 255, 255));
				}
				nk_draw_text(canvas, label_rect,
							 entityNames[i].c_str(), (int)entityNames[i].length(),
							 ctx->style.font,
							 (i == selectedIndex ? nk_rgb(255,255,255) : nk_rgb(30,30,30)),
							 nk_rgb(230,230,230));

				occupiedRects.push_back(label_rect);

				// ----- Drag detection: on mouse-down in label area, begin potential drag -----
				if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) &&
					nk_input_is_mouse_hovering_rect(&ctx->input, label_rect) &&
					dragIndex < 0)
				{
					dragIndex = i;
					dragStartPos = mousePos;
					dragging = false;
				}
				// If already pressed on this index, check move threshold to start dragging
				if (dragIndex == i && !dragging && mouseDown) {
					float dx = mousePos.x - dragStartPos.x;
					float dy = mousePos.y - dragStartPos.y;
					float drag_threshold = 5.0f * ui_scale; // Scale drag threshold
					if (sqrt(dx*dx + dy*dy) > drag_threshold) {
						dragging = true;
						selectedIndex = dragIndex;
					}
				}
				// Handle click (no drag) for selection
				if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, label_rect)) {
					if (!dragging) {
						selectedIndex = i;
						printf("Selected entity: '%s' (level %d, index %d)\n",
							   entityNames[i].c_str(), entityLevels[i], i);
					}
				}
			}

			nk_layout_space_end(ctx);

			// Ghost during drag - scale ghost dimensions and offset
			if (dragging && dragIndex >= 0 && dragIndex < (int)entityNames.size()) {
				float ghost_width = 100.0f * ui_scale;
				float ghost_offset = 8.0f * ui_scale;
				float ghost_radius = 4.0f * ui_scale;
				
				struct nk_rect ghost = nk_rect(mousePos.x + ghost_offset, mousePos.y + ghost_offset, ghost_width, row_h);
				nk_fill_rect(canvas, ghost, ghost_radius, nk_rgba(150,150,150,128));
				nk_draw_text(canvas, ghost, entityNames[dragIndex].c_str(),
							 (int)entityNames[dragIndex].length(),
							 ctx->style.font, nk_rgb(0,0,0), nk_rgb(230,230,230));
			}

			// Handle drop on release
			if (mouseReleased && dragIndex >= 0) {
				if (dragging) {
					// Determine drop target by re-iterating labels
					int targetIndex = -1;
					for (int j = 0; j < (int)entityNames.size(); ++j) {
						int lvl = entityLevels[j];
						float y_pos_check = j * row_h;
						struct nk_rect lbl = nk_rect(
							group_bounds.x + x_offset + lvl*step + text_pad,
							group_bounds.y + y_pos_check,
							side_w - (x_offset + lvl*step) - text_pad - 20 * ui_scale, // Scale scrollbar space
							row_h
						);
						if (mousePos.x >= lbl.x && mousePos.x < lbl.x + lbl.w &&
							mousePos.y >= lbl.y && mousePos.y < lbl.y + lbl.h)
						{
							targetIndex = j;
							break;
						}
					}
					int src = dragIndex;
					if (targetIndex >= 0 && targetIndex != src && src < (int)entityLevels.size()) {
						int srcLevel = entityLevels[src];
						int end = src+1;
						while (end < (int)entityNames.size() && entityLevels[end] > srcLevel) end++;
						// Prevent dropping onto own subtree
						if (!(targetIndex >= src && targetIndex < end)) {
							// Extract subtree
							int subtreeLen = end - src;
							std::vector<std::string> namesSub(entityNames.begin()+src, entityNames.begin()+end);
							std::vector<int> levelsSub(entityLevels.begin()+src, entityLevels.begin()+end);
							std::vector<int> indexesSub(entityIndexes.begin()+src, entityIndexes.begin()+end);
							
							int oldParentLevel = levelsSub[0];
							// Remove old subtree
							entityNames.erase(entityNames.begin()+src, entityNames.begin()+end);
							entityLevels.erase(entityLevels.begin()+src, entityLevels.begin()+end);
							entityIndexes.erase(entityIndexes.begin()+src, entityIndexes.begin()+end);
							// Adjust targetIndex if needed
							int insertAfter = targetIndex;
							if (targetIndex > src) insertAfter = targetIndex - subtreeLen;
							// New parent level
							int parentLevel = entityLevels[insertAfter];
							// Find insertion index after its descendants
							int insertIndex = insertAfter + 1;
							while (insertIndex < (int)entityNames.size() && entityLevels[insertIndex] > parentLevel) {
								insertIndex++;
							}
							int levelDelta = (parentLevel + 1) - oldParentLevel;
							// Insert subtree
							for (int k = 0; k < subtreeLen; ++k) {
								entityNames.insert(entityNames.begin() + insertIndex + k, namesSub[k]);
								entityLevels.insert(entityLevels.begin() + insertIndex + k, levelsSub[k] + levelDelta);
								entityIndexes.insert(entityIndexes.begin() + insertIndex + k, indexesSub[k]);
							}
							selectedIndex = insertIndex;
							printf("Moved '%s' subtree under '%s'\n",
								   namesSub[0].c_str(), entityNames[insertAfter].c_str());
						} else {
							printf("Invalid drop: cannot move onto own descendant\n");
						}
					}
				}
				dragging = false;
				dragIndex = -1;
			}

			nk_group_end(ctx);
		}

		// Clear selection only on clicks in empty space, and only print if something was selected
		if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, side_rect)) {
			struct nk_vec2 mp = ctx->input.mouse.pos;
			bool inOccupied = false;
			for (auto &r : occupiedRects) {
				if (mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h) {
					inOccupied = true; break;
				}
			}
			if (!inOccupied) {
				if (selectedIndex != -1) {
					selectedIndex = -1;
					printf("Cleared selection\n");
				}
			}
		}
	}
	nk_end(ctx);
	
	// Restore original font height
	font->height = original_font_height;
	
	selected_item = selectedIndex;
	end_selection = selected_item+1;

	if (selected_item < 0 || selected_item >= (int)entityLevels.size()) return;
	
	while (end_selection < (int)entityNames.size() && entityLevels[end_selection] > entityLevels[selected_item]) {
		end_selection++;
	}
}
void DrawProperties(Runtime::EditorRuntime *editor, int side_x, int sidebar_h, int menu_height, int side_w, int prop_h) {
	struct nk_rect prop_rect = nk_rect((float)side_x, (float)(menu_height + sidebar_h), (float)side_w, (float)prop_h);
	
	if (nk_begin(ctx, "Properties", prop_rect, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
		nk_layout_row_dynamic(ctx, 20, 1);
		nk_label(ctx, "Properties", NK_TEXT_CENTERED);

		float posX=0.0f, posY=0.0f, posZ=0.0f;
		
		if (0 <= selected_item && selected_item < entityIndexes.size()){
			int currentmesh = entityIndexes[selected_item];
			
			if (currentmesh > 0 && currentmesh < editor->meshes.size()) {
				std::shared_ptr<Mesh> baseMesh = editor->meshes[currentmesh];
				
				posX = baseMesh->transform.position.x;
				posY = baseMesh->transform.position.y;
				posZ = baseMesh->transform.position.z;
			}
		}
		
		char bufX[32], bufY[32], bufZ[32];
//		bool init=false;
//		if (!init) {
		snprintf(bufX,32,"%.4f",posX);
		snprintf(bufY,32,"%.4f",posY);
		snprintf(bufZ,32,"%.4f",posZ);
//			init=true;
//		}
		

		auto draw_field=[&](const char* label, float& val, char* buf) {
			const float rowH=22.0f, lblW=80.0f, btnW=20.0f, fldW=80.0f;
			float oldVal = val;
			
			nk_layout_row_begin(ctx, NK_STATIC, rowH, 4);
			nk_layout_row_push(ctx,lblW);
			nk_label(ctx,label,NK_TEXT_LEFT);
			
			nk_layout_row_push(ctx,btnW);
			if (nk_button_symbol(ctx,NK_SYMBOL_TRIANGLE_LEFT)) {
				val=std::clamp(val-0.1f,-100.0f,100.0f);
				snprintf(buf,32,"%.4f",val);
			}
			
			nk_layout_row_push(ctx,fldW);
			if (nk_edit_string_zero_terminated(ctx,NK_EDIT_FIELD|NK_EDIT_CLIPBOARD,buf,32,nk_filter_float)) {
				val=strtof(buf,nullptr);
				snprintf(buf,32,"%.4f",val);
			}
			
			nk_layout_row_push(ctx,btnW);
			if (nk_button_symbol(ctx,NK_SYMBOL_TRIANGLE_RIGHT)) {
				val=std::clamp(val+0.1f,-100.0f,100.0f);
				snprintf(buf,32,"%.4f",val);
			}
			nk_layout_row_end(ctx);
			
			return val;
		};
		
		float x = draw_field("Position X", posX, bufX);
		float y = draw_field("Position Y", posY, bufY);
		float z = draw_field("Position Z", posZ, bufZ);
		
		if (0 <= selected_item && selected_item < entityIndexes.size()){
			int currentmesh = entityIndexes[selected_item];
			
			if (currentmesh > 0 && currentmesh < editor->meshes.size()) {
				std::shared_ptr<Mesh> baseMesh = editor->meshes[currentmesh];
				
				float bmx = baseMesh->transform.position.x;
				float bmy = baseMesh->transform.position.y;
				float bmz = baseMesh->transform.position.z;
				
				if (bmx != x || bmy != y || bmz != z){
					float dx = x-bmx;
					float dy = y-bmy;
					float dz = z-bmz;
					
					for (int i = selected_item; i < end_selection; i++) {
						int thismesh = entityIndexes[i];
						
						printf("Trying this: %d\n", thismesh);
						
						if (thismesh < 0 || thismesh >= editor->meshes.size()) {
							continue;
						}
						
						printf("Continueing\n");
						
						auto mesh = editor->meshes[thismesh];
						mesh->transform.position.x += dx;
						mesh->transform.position.y += dy;
						mesh->transform.position.z += dz;
						Renderer::RendererManager::UpdateMesh(thismesh, mesh);
					}
				}
			}
		}
	}
	nk_end(ctx);
}

int GetSelectedMeshIndex() {
    if (selected_item < 0 || selected_item >= (int)entityIndexes.size())
        return -1;
    return entityIndexes[selected_item];
}

std::vector<int> GetSelectedMeshSubtreeIndices() {
    std::vector<int> out;
    if (selected_item < 0 || selected_item >= (int)entityIndexes.size())
        return out;
    for (int i = selected_item; i < end_selection && i < (int)entityIndexes.size(); ++i) {
        out.push_back(entityIndexes[i]);
    }
    return out;
}

} // namespace EditorPanels