// Editor.h
#pragma once
#include "Renderer/Mesh.h"
#include "Runtime.h"
#include <memory>
#include <vector>

namespace Runtime {
    // Struct to hold image frame information
    struct ViewFrameInfo {
        int x, y, width, height;
        bool isVisible;
        
        ViewFrameInfo() : x(0), y(0), width(0), height(0), isVisible(false) {}
        ViewFrameInfo(int x, int y, int w, int h, bool visible = true) 
            : x(x), y(y), width(w), height(h), isVisible(visible) {}
            
        // Helper method to check if a point is within the frame
        bool ContainsPoint(int px, int py) const {
            return isVisible && (px >= x && px < x + width && py >= y && py < y + height);
        }
    };

    class EditorRuntime : public Runtime {
    public:
        bool Init() override;
        void PrepareForFrameRender() override;
        void Cleanup() override;
		bool windowResized = false;
       
        virtual void ProcessInput(GLFWwindow* window, float deltaTime) override;
        virtual void ProcessInput(float xpos, float ypos, std::function<bool(int)> KeyIsDown, float deltaTime) override;
       
        int AddMesh(std::string filepath, glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 rot = glm::vec3(0, 0, 0));
        bool DeleteMesh(int meshindex);
       
        // Image frame access methods
        ViewFrameInfo GetViewFrameInfo() const { return currentFrameInfo; }
        bool IsPointInImageFrame(int x, int y) const { return currentFrameInfo.ContainsPoint(x, y); }
        
        // Individual getters if you prefer them
        int GetImageFrameX() const { return currentFrameInfo.x; }
        int GetImageFrameY() const { return currentFrameInfo.y; }
        int GetImageFrameWidth() const { return currentFrameInfo.width; }
        int GetImageFrameHeight() const { return currentFrameInfo.height; }
        bool IsImageFrameVisible() const { return currentFrameInfo.isVisible; }
       
        std::vector<std::shared_ptr<Mesh>> meshes; // might as well? IDK
        
    private:
        ViewFrameInfo currentFrameInfo;
    };
}