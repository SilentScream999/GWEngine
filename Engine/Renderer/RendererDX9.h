// RendererDX9.h
#pragma once
#include "IRenderer.h"
#include "Mesh.h"
#include <memory>
#include <windows.h>
#include <d3d9.h>
#include <glm/glm.hpp>

namespace Renderer {

class RendererDX9 : public IRenderer {
public:
	bool Init(Camera *cam) override;
	void RenderFrame() override;
	bool SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) override;

private:
	HWND                               hwnd       = nullptr;
	LPDIRECT3D9                        d3d        = nullptr;
	LPDIRECT3DDEVICE9                  d3dDevice  = nullptr;
	D3DPRESENT_PARAMETERS              pp         = {};    // keep for resets
	UINT                               width      = 800;  // current backbuffer size
	UINT                               height     = 600;

	std::vector<std::shared_ptr<Mesh>> meshes;
	UINT                               numberOfMeshVertexes;
	LPDIRECT3DVERTEXBUFFER9            vb         = nullptr;
	LPDIRECT3DINDEXBUFFER9             ib         = nullptr;
	UINT                               numIndices = 0;

	double                             lastTime   = 0.0;
	float                              angle      = 0.0f;

	// Convert glm::mat4 (column-major) to D3DMATRIX (row-major)
	static D3DMATRIX ToD3D(const glm::mat4& m) { // you did this manually!! why sweet god why? there better be a reason for this...
		return D3DMATRIX{
			m[0][0], m[0][1], m[0][2], m[0][3],
			m[1][0], m[1][1], m[1][2], m[1][3],
			m[2][0], m[2][1], m[2][2], m[2][3],
			m[3][0], m[3][1], m[3][2], m[3][3]
		};
	}

	// Called on WM_SIZE to re-create the backbuffer at the new size
	void ResetDevice();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Vertex format matching your Mesh::Vertex
struct DXVertex {
	float x,y,z;
	float nx,ny,nz;
	float u,v;
};
#define D3DFVF_DXVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

} // namespace Renderer
