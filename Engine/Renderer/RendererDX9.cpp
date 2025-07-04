// RendererDX9.cpp
#include "RendererDX9.h"
#include "../Core/Logger.h"
#include <GLFW/glfw3.h>               // for timing
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <windows.h>
#include <vector>
#include "../Core/stb_impl.h"

using namespace Renderer;

//-----------------------------------------------------------------------------
// raw mouse deltas
static float g_rawMouseX = 0.0f;
static float g_rawMouseY = 0.0f;

struct SkyboxVertex {
	float x, y, z;
	float u, v;  // Texture coordinates
};

static SkyboxVertex skyboxVertices[36];
static IDirect3DVertexBuffer9* skyboxVB = nullptr;
static IDirect3DTexture9* skyboxTexture = nullptr;

void CreateSkyboxVertices() {
	// Cube positions with corresponding texture coordinates
	struct VertexData {
		float x, y, z, u, v;
	};
	
	// these uvs are set so that they always fit the texture
	VertexData vertexData[36] = {
		// Front face (negative Z),
		-1.0,   -1.0,   -1.0,   0.25,   0.6666666666666666,
		1.0,    -1.0,   -1.0,   0.5,    0.6666666666666666,
		1.0,    1.0,    -1.0,   0.5,    0.3333333333333333,
		1.0,    1.0,    -1.0,   0.5,    0.3333333333333333,
		-1.0,   1.0,    -1.0,   0.25,   0.3333333333333333,
		-1.0,   -1.0,   -1.0,   0.25,   0.6666666666666666,
		
		
		// Back face (positive Z),
		-1.0,   -1.0,   1.0,    0.999,    0.665,
		1.0,    -1.0,   1.0,    0.75,     0.665,
		1.0,    1.0,    1.0,    0.75,     0.334,
		1.0,    1.0,    1.0,    0.75,     0.334,
		-1.0,   1.0,    1.0,    0.999,    0.334,
		-1.0,   -1.0,   1.0,    0.999,    0.665,
		
		
		// Left face (negative X),
		-1.0,   1.0,    1.0,    0.001,   0.334,
		-1.0,   1.0,    -1.0,   0.25,   0.334,
		-1.0,   -1.0,   -1.0,   0.25,   0.665,
		-1.0,   -1.0,   -1.0,   0.25,   0.665,
		-1.0,   -1.0,   1.0,    0.001,   0.665,
		-1.0,   1.0,    1.0,    0.001,   0.334,
		
		
		// Right face (positive X),
		1.0,    1.0,    1.0,    0.75,   0.334,
		1.0,    1.0,    -1.0,   0.5,    0.334,
		1.0,    -1.0,   -1.0,   0.5,    0.665,
		1.0,    -1.0,   -1.0,   0.5,    0.665,
		1.0,    -1.0,   1.0,    0.75,   0.665,
		1.0,    1.0,    1.0,    0.75,   0.334,
		
		
		// Bottom face (negative Y),
		-1.0,   -1.0,   -1.0,   0.251,   0.6666666666666666,
		1.0,    -1.0,   -1.0,   0.499,   0.6666666666666666,
		1.0,    -1.0,   1.0,    0.499,   0.999,
		1.0,    -1.0,   1.0,    0.499,   0.999,
		-1.0,   -1.0,   1.0,    0.251,   0.999,
		-1.0,   -1.0,   -1.0,   0.251,   0.6666666666666666,
		
		
		// Top face (positive Y),
		-1.0,   1.0,    -1.0,   0.251,   0.3333333333333333,
		1.0,    1.0,    -1.0,   0.499,   0.3333333333333333,
		1.0,    1.0,    1.0,    0.499,   0.001,
		1.0,    1.0,    1.0,    0.499,   0.001,
		-1.0,   1.0,    1.0,    0.251,   0.001,
		-1.0,   1.0,    -1.0,   0.251,   0.3333333333333333,
	};
	
	// Copy vertex data
	for (int i = 0; i < 36; i++) {
		skyboxVertices[i].x = vertexData[i].x;
		skyboxVertices[i].y = vertexData[i].y;
		skyboxVertices[i].z = vertexData[i].z;
		skyboxVertices[i].u = vertexData[i].u;
		skyboxVertices[i].v = vertexData[i].v;
	}
}

//-----------------------------------------------------------------------------
// Force the cursor to be truly visible or hidden, fixing WinAPI counter issues
void ForceShowCursor(bool show) {
	int counter;
	do {
		counter = ShowCursor(show);
	} while ((show && counter < 0) || (!show && counter >= 0));
}

//------------------------------------------------------------------------
LRESULT CALLBACK RendererDX9::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CREATE) {
		auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
	}
	RendererDX9* self = reinterpret_cast<RendererDX9*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (msg) {
	case WM_INPUT: {
		UINT size = 0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		if (size > 0) {
			std::vector<BYTE> buf(size);
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
				RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());
				if (raw->header.dwType == RIM_TYPEMOUSE) {
					g_rawMouseX -= static_cast<float>(raw->data.mouse.lLastX);
					g_rawMouseY -= static_cast<float>(raw->data.mouse.lLastY);
				}
			}
		}
		return 0;
	}

	case WM_DESTROY:
		ClipCursor(nullptr);
		ForceShowCursor(true);
		PostQuitMessage(0);
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE && self) {
			ClipCursor(nullptr);
			ForceShowCursor(true);
			self->mouseCaptured     = false;
			self->mouseInputEnabled = false;
		}
		return 0;

	case WM_LBUTTONDOWN:
		if (self && !self->mouseCaptured) {
			RECT rc; GetClientRect(self->hwnd, &rc);
			POINT ul = { rc.left, rc.top }, lr = { rc.right, rc.bottom };
			ClientToScreen(self->hwnd, &ul);
			ClientToScreen(self->hwnd, &lr);
			self->clipRect = { ul.x, ul.y, lr.x, lr.y };
			ClipCursor(&self->clipRect);
			ForceShowCursor(false);

			g_rawMouseX = g_rawMouseY = 0.0f;
			SetCursorPos(self->centerScreenPos.x, self->centerScreenPos.y);

			self->mouseCaptured     = true;
			self->mouseInputEnabled = true;
		}
		return 0;

	case WM_SETFOCUS:
		if (self && self->mouseCaptured) {
			ClipCursor(&self->clipRect);
			ForceShowCursor(false);
		}
		return 0;

	case WM_KILLFOCUS:
		if (self) {
			ClipCursor(nullptr);
			ForceShowCursor(true);
			self->mouseCaptured     = false;
			self->mouseInputEnabled = false;
		}
		return 0;

	case WM_SIZE:
		if (self && self->d3dDevice && wParam != SIZE_MINIMIZED) {
			// update stored size and reset device
			self->width  = LOWORD(lParam);
			self->height = HIWORD(lParam);
			self->ResetDevice();
		}
		return 0;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

//------------------------------------------------------------------------
bool RendererDX9::Init(Camera* c, Runtime::Runtime *r) {
	runtime = r;
	cam = c;
	HINSTANCE hInst = GetModuleHandle(nullptr);

	WNDCLASS wc = {};
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInst;
	wc.lpszClassName = "DX9WindowClass";
	RegisterClass(&wc);

	hwnd = CreateWindow(
		"DX9WindowClass", "HL2-Style Engine - DirectX 9",
		WS_OVERLAPPEDWINDOW, 100, 100,
		static_cast<int>(width), static_cast<int>(height),
		nullptr, nullptr, hInst, this
	);
	if (!hwnd) {
		Logger::Error("Failed to create Win32 window.");
		return false;
	}

	{
		RECT rc; GetClientRect(hwnd, &rc);
		POINT ul = { rc.left, rc.top }, lr = { rc.right, rc.bottom };
		ClientToScreen(hwnd, &ul);
		ClientToScreen(hwnd, &lr);
		clipRect = { ul.x, ul.y, lr.x, lr.y };

		POINT ctr = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
		ClientToScreen(hwnd, &ctr);
		centerScreenPos = ctr;

		ClipCursor(&clipRect);
		ForceShowCursor(false);
		mouseCaptured     = true;
		mouseInputEnabled = true;
	}

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;
	rid.usUsage     = 0x02;
	rid.dwFlags     = RIDEV_INPUTSINK;
	rid.hwndTarget  = hwnd;
	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
		Logger::Warn("Failed to register raw mouse input.");
	}

	if (!glfwInit()) {
		Logger::Error("Failed to initialize GLFW.");
	}
	lastTime = glfwGetTime();

	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d) {
		Logger::Error("Failed to create D3D9 object.");
		return false;
	}

	ZeroMemory(&pp, sizeof(pp));
	pp.Windowed               = TRUE;
	pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat       = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth        = width;
	pp.BackBufferHeight       = height;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;

	// initial MSAA setup
	D3DMULTISAMPLE_TYPE msType    = D3DMULTISAMPLE_4_SAMPLES;
	DWORD               msQuality = 0;
	if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
			pp.BackBufferFormat, pp.Windowed,
			msType, &msQuality
		))) {
		pp.MultiSampleType    = msType;
		pp.MultiSampleQuality = (msQuality > 0 ? msQuality - 1 : 0);
	} else {
		Logger::Warn("4× MSAA not supported; falling back.\n");
	}

	if (FAILED(d3d->CreateDevice(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&pp, &d3dDevice
		))) {
		Logger::Error("Failed to create D3D9 device.");
		return false;
	}

	// render states
	d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,  TRUE);
	d3dDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);
	d3dDevice->SetRenderState(D3DRS_ZENABLE,               D3DZB_TRUE);
	d3dDevice->SetRenderState(D3DRS_CULLMODE,              D3DCULL_CW);

	// initial viewport
	D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };
	d3dDevice->SetViewport(&vp);

	ShowWindow(hwnd, SW_SHOW);
	Logger::Info("DX9 device created with MSAA.");
	
	// this is all about skybox
	
	CreateSkyboxVertices();
	
	// Create vertex buffer with new FVF format
	HRESULT hr = d3dDevice->CreateVertexBuffer(
		36 * sizeof(SkyboxVertex),
		D3DUSAGE_WRITEONLY,
		D3DFVF_XYZ | D3DFVF_TEX1,  // Position + 1 set of texture coordinates
		D3DPOOL_MANAGED,
		&skyboxVB,
		nullptr
	);
	
	if (FAILED(hr)) {
		Logger::Error("Failed to create skybox vertex buffer - HRESULT: " + std::to_string(hr));
		return false;
	}
	
	// Fill vertex buffer
	void* pVertices;
	hr = skyboxVB->Lock(0, 0, &pVertices, 0);
	if (FAILED(hr)) {
		Logger::Error("Failed to lock skybox vertex buffer");
		skyboxVB->Release();
		skyboxVB = nullptr;
		return false;
	}
	
	memcpy(pVertices, skyboxVertices, 36 * sizeof(SkyboxVertex));
	skyboxVB->Unlock();
	
	const std::string& texturePath = "assets/textures/skybox.bmp";
	
	// Load skybox texture
	int width, height, channels;
	unsigned char* imageData = stb_impl::LoadImageFromFile(texturePath.c_str(), &width, &height, &channels);
	
	if (!imageData) {
		Logger::Error("Failed to load image data: " + texturePath);
		skyboxVB->Release();
		skyboxVB = nullptr;
		return false;
	}
	
	hr = d3dDevice->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &skyboxTexture, nullptr);
	if (FAILED(hr)) {
		Logger::Error("Failed to create texture");
		stb_impl::FreeImageData(imageData);
		skyboxVB->Release();
		skyboxVB = nullptr;
		return false;
	}
	
	// Lock texture and copy data
	D3DLOCKED_RECT lockedRect;
	hr = skyboxTexture->LockRect(0, &lockedRect, nullptr, 0);
	if (SUCCEEDED(hr)) {
		// Convert image data to D3DFMT_A8R8G8B8 format (ARGB)
		DWORD* dest = (DWORD*)lockedRect.pBits;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				int srcIndex = (y * width + x) * channels;
				DWORD pixel;
				
				if (channels == 3) { // RGB
					pixel = D3DCOLOR_XRGB(imageData[srcIndex], imageData[srcIndex + 1], imageData[srcIndex + 2]);
				} else if (channels == 4) { // RGBA
					pixel = D3DCOLOR_ARGB(imageData[srcIndex + 3], imageData[srcIndex], imageData[srcIndex + 1], imageData[srcIndex + 2]);
				} else {
					pixel = D3DCOLOR_XRGB(128, 128, 128); // Default gray
				}
				
				dest[y * (lockedRect.Pitch / 4) + x] = pixel;
			}
		}
		skyboxTexture->UnlockRect(0);
	}
	
	// sample colors from the skybox for lighting
	
	float xPoints[12] = {
		// horizontals
		0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
		// top - no bottom
		0.38
	};
	float yPoints[12] = {
		// horizontals
		0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
		// top - no bottom
		0.17
	};
	
	float totalr = 0.0;
	float totalg = 0.0;
	float totalb = 0.0;
	
	float totalstrength = 0.0;
	
	for (int i = 0; i < 12; i++) {
		float x = xPoints[i];
		float y = yPoints[i];
	
		float strength = (1.0-y)*(1.0-y);
		totalstrength += strength;
	
		// Convert UV to pixel coordinates
		int pixelX = (int)(x * (width - 1));
		int pixelY = (int)(y * (height - 1));
		int srcIndex = (pixelY * width + pixelX) * channels;
		
		totalr += imageData[srcIndex] * strength;
		totalg += imageData[srcIndex+1] * strength;
		totalb += imageData[srcIndex+2] * strength;
	}
	
	skybox_r = (totalr/totalstrength)/255.0f;
	skybox_g = (totalg/totalstrength)/255.0f;
	skybox_b = (totalb/totalstrength)/255.0f;
	
	stb_impl::FreeImageData(imageData);
	
	Logger::Info("Skybox initialized successfully");
	
	return true;
}

//------------------------------------------------------------------------
void RendererDX9::ResetDevice() {
	pp.BackBufferWidth  = width;
	pp.BackBufferHeight = height;

	// re-check MSAA
	D3DMULTISAMPLE_TYPE msType    = D3DMULTISAMPLE_4_SAMPLES;
	DWORD               msQuality = 0;
	if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
			pp.BackBufferFormat, pp.Windowed,
			msType, &msQuality
		))) {
		pp.MultiSampleType    = msType;
		pp.MultiSampleQuality = (msQuality > 0 ? msQuality - 1 : 0);
	} else {
		Logger::Warn("4× MSAA not supported on Reset; falling back.\n");
		pp.MultiSampleType    = D3DMULTISAMPLE_NONE;
		pp.MultiSampleQuality = 0;
	}

	if (FAILED(d3dDevice->Reset(&pp))) {
		Logger::Error("Failed to reset D3D device on resize.");
		return;
	}

	// re-apply render states
	d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,  TRUE);
	d3dDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);
	d3dDevice->SetRenderState(D3DRS_ZENABLE,               D3DZB_TRUE);
	d3dDevice->SetRenderState(D3DRS_CULLMODE,              D3DCULL_CW);

	// reset viewport to new size
	D3DVIEWPORT9 vp;
	vp.X      = 0;
	vp.Y      = 0;
	vp.Width  = width;
	vp.Height = height;
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;
	d3dDevice->SetViewport(&vp);
}

int RendererDX9::AddMesh(std::shared_ptr<Mesh> mesh) {
	for (int i = 0; i < meshes.size(); i++) {
		if (!meshes[i]) {
			UpdateMesh(i, mesh);
			return i;
		}
	}
	
	int indx = meshes.size();
	UpdateMesh(indx, mesh);
	return indx;
}

bool RendererDX9::UpdateMesh(int indx, std::shared_ptr<Mesh> mesh) {
	// Ensure the vectors are large enough
	if (indx >= meshes.size()) {
		meshes.resize(indx + 1);
		meshBuffers.resize(indx + 1);
	}
	
	meshes[indx] = mesh;
	
	int vertexCount = mesh->vertices.size();
	DX9MeshData& meshData = meshBuffers[indx];
	
	// Check if we need to recreate the vertex buffer
	bool needNewVertexBuffer = false;
	if (!meshData.vertexBuffer) {
		needNewVertexBuffer = true;
	} else if (meshData.vertexCount != vertexCount) {
		// Size changed, need new buffer
		needNewVertexBuffer = true;
		meshData.vertexBuffer->Release();
		meshData.vertexBuffer = nullptr;
	}
	
	// Create or reuse vertex buffer
	if (needNewVertexBuffer) {
		HRESULT hr = d3dDevice->CreateVertexBuffer(
			vertexCount * sizeof(DXVertex),
			D3DUSAGE_WRITEONLY,
			D3DFVF_DXVERTEX,
			D3DPOOL_MANAGED,
			&meshData.vertexBuffer,
			nullptr
		);
		
		if (FAILED(hr)) {
			Logger::Error("Failed to create vertex buffer for mesh");
			return false;
		}
	}
	
	// Always update the vertex data (even if buffer was reused)
	DXVertex* verts;
	meshData.vertexBuffer->Lock(0, 0, (void**)&verts, 0);
	for (int i = 0; i < vertexCount; i++) {
		const auto& v = mesh->vertices[i];
		verts[i].x = v.position.x;
		verts[i].y = v.position.y;
		verts[i].z = v.position.z;
		
		verts[i].nx = v.normal.x;
		verts[i].ny = v.normal.y;
		verts[i].nz = v.normal.z;  // Fix: was nx again
		
		// Add UV coordinates if you have them:
		// verts[i].u = v.uv.x;
		// verts[i].v = v.uv.y;
	}
	meshData.vertexBuffer->Unlock();
	
	// Check if we need to recreate the index buffer
	bool needNewIndexBuffer = false;
	if (!meshData.indexBuffer) {
		needNewIndexBuffer = true;
	} else if (meshData.vertexCount != vertexCount) {
		// Size changed, need new buffer
		needNewIndexBuffer = true;
		meshData.indexBuffer->Release();
		meshData.indexBuffer = nullptr;
	}
	
	// Create or reuse index buffer
	if (needNewIndexBuffer) {
		HRESULT hr = d3dDevice->CreateIndexBuffer(
			vertexCount * sizeof(WORD),
			D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_MANAGED,
			&meshData.indexBuffer,
			nullptr
		);
		
		if (FAILED(hr)) {
			Logger::Error("Failed to create index buffer for mesh");
			return false;
		}
		
		// Fill index buffer with sequential indices (only when creating new buffer)
		WORD* indices;
		meshData.indexBuffer->Lock(0, 0, (void**)&indices, 0);
		for (int i = 0; i < vertexCount; i++) {
			indices[i] = i;
		}
		meshData.indexBuffer->Unlock();
	}
	
	// Update metadata
	meshData.vertexCount = vertexCount;
	meshData.triangleCount = vertexCount / 3;
	
	return true;
}

bool RendererDX9::DeleteMesh(int indx) {
	// Check if index is valid
	if (indx < 0 || indx >= meshBuffers.size()) {
		Logger::Error("Invalid mesh index for deletion");
		return false;
	}
	
	// Get reference to the mesh data
	DX9MeshData& meshData = meshBuffers[indx];
	
	// Release buffers
	if (meshData.vertexBuffer) {
		meshData.vertexBuffer->Release();
	}
	if (meshData.indexBuffer) {
		meshData.indexBuffer->Release();
	}
	
	meshes[indx] = nullptr;
	
	return true;
}
	

//------------------------------------------------------------------------
bool RendererDX9::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
	meshes = std::move(msh);
	
	meshBuffers.clear();
	meshBuffers.resize(meshes.size());
	
	for (int meshi = 0; meshi < meshes.size(); meshi++) {
		const auto& mesh = meshes[meshi];
		UpdateMesh(meshi, mesh);
	}

	return true;
}

bool RendererDX9::KeyIsDown(int key) {
	return GetAsyncKeyState(key) & 0x8000;
}

//------------------------------------------------------------------------
void RendererDX9::HandleInputDX9(float dt) {
	// skip if not capturing or input disabled
	if (!hwnd || !cam || !mouseCaptured || !mouseInputEnabled)
		return;

	// warp cursor back to center each frame
	SetCursorPos(centerScreenPos.x, centerScreenPos.y);

	// this takes a lambda which calls the KeyIsDown function because c++ is strange and this is the bestish way
	runtime->ProcessInput(g_rawMouseX, g_rawMouseY, [this](int key){ return KeyIsDown(key); }, dt);
	//cam->ProcessInput(g_rawMouseX, g_rawMouseY, [this](int key){ return KeyIsDown(key); }, dt);

	g_rawMouseX = g_rawMouseY = 0.0f;
}

void Renderer::RendererDX9::setSize(int newWidth, int newHeight) {
	if (!hwnd) return;

	// Store new size
	width = newWidth;
	height = newHeight;

	// Resize the actual window (this sends a WM_SIZE message too)
	SetWindowPos(
		hwnd, nullptr,
		0, 0, width, height,
		SWP_NOZORDER | SWP_NOMOVE
	);

	// Optional: Reset device right away (if not relying solely on WM_SIZE)
	ResetDevice();
}

Renderer::ImageData Renderer::RendererDX9::CaptureFrame() {
	return ImageData();
}

void RendererDX9::RenderSkybox() {
	// Check if skybox is ready
	if (!skyboxVB || !skyboxTexture) {
		Logger::Error("Skybox not properly initialized");
		return;
	}
	
	// Save current render states
	DWORD oldZWrite, oldLighting, oldCullMode;
	d3dDevice->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWrite);
	d3dDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
	d3dDevice->GetRenderState(D3DRS_CULLMODE, &oldCullMode);
	
	// Setup skybox render states
	d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	
	// Set texture and texture stage states
	d3dDevice->SetTexture(0, skyboxTexture);
	d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	
	// Set sampling states for better quality
	d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	
	// Set vertex format and stream
	HRESULT hr1 = d3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	HRESULT hr2 = d3dDevice->SetStreamSource(0, skyboxVB, 0, sizeof(SkyboxVertex));
	
	if (FAILED(hr1) || FAILED(hr2)) {
		Logger::Error("Failed to set skybox vertex format or stream");
		// Restore states before returning
		d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
		d3dDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
		d3dDevice->SetRenderState(D3DRS_CULLMODE, oldCullMode);
		return;
	}
	
	// Draw skybox
	HRESULT hr3 = d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 12);
	if (FAILED(hr3)) {
		Logger::Error("Failed to draw skybox primitive");
	}
	
	// Clean up texture state
	d3dDevice->SetTexture(0, nullptr);
	
	// Restore render states
	d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
	d3dDevice->SetRenderState(D3DRS_CULLMODE, oldCullMode);
	
	// Restore your original vertex format for the meshes
	d3dDevice->SetFVF(D3DFVF_DXVERTEX);
}

//------------------------------------------------------------------------
void RendererDX9::RenderFrame() {
	if (!d3dDevice) return;

	// pump Win32 messages - yeah! pump those messages!
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) exit(0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// timing
	double now = glfwGetTime();
	float dt = static_cast<float>(now - lastTime);
	lastTime = now;

	HandleInputDX9(dt);
	cam->updateForFrame();

	// clear & begin scene
	d3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(25,25,25));
	d3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(20,20,50), 1.0f, 0);
	d3dDevice->BeginScene();
	
	// projection
	float aspect = static_cast<float>(width) / static_cast<float>(height);
	glm::mat4 projGL = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
	auto mtr = ToD3D(projGL);
	d3dDevice->SetTransform(D3DTS_PROJECTION, &mtr);

	// view
	glm::mat4 viewGL = glm::lookAt(cam->transform.position, cam->lookAtPosition, glm::vec3(0, 1, 0));
	
	
	// === RENDER SKYBOX ===
	// Set skybox view matrix (rotation only, no translation)
	glm::mat4 skyboxViewGL = glm::mat4(glm::mat3(viewGL));
	auto skyboxViewMtr = ToD3D(skyboxViewGL);
	d3dDevice->SetTransform(D3DTS_VIEW, &skyboxViewMtr);
	
	// Set skybox world matrix to identity (no transformation)
	D3DMATRIX identityMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	d3dDevice->SetTransform(D3DTS_WORLD, &identityMatrix);
	
	RenderSkybox();
	
	auto mtr2 = ToD3D(viewGL);
	d3dDevice->SetTransform(D3DTS_VIEW, &mtr2);
	
	// material + lighting
	D3DMATERIAL9 mat = {};
	mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = 1.0f; // this is the color of the object
	
	mat.Ambient.r = skybox_r*.5f;
	mat.Ambient.g = skybox_g*.5f;
	mat.Ambient.b = skybox_b*.5f;
	
	mat.Diffuse.a = mat.Ambient.a = 1.0f;
	d3dDevice->SetMaterial(&mat);

	D3DLIGHT9 light = {};
	light.Type      = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = skybox_r;
	light.Diffuse.g = skybox_g;
	light.Diffuse.b = skybox_b;
	light.Ambient.r = skybox_r;
	light.Ambient.g = skybox_g;
	light.Ambient.b = skybox_b;
	
	light.Direction = {0.0f, -1.0f, 0.0f}; // straight down
	d3dDevice->SetLight(0, &light);
	d3dDevice->LightEnable(0, TRUE);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
	
	for (size_t i = 0; i < meshes.size() && i < meshBuffers.size(); i++) {
		const auto& mesh = meshes[i];
		
		if (!mesh) {
			continue;
		}
		
		const auto& meshData = meshBuffers[i];
		
		// Set this mesh's buffers
		d3dDevice->SetStreamSource(0, meshData.vertexBuffer, 0, sizeof(DXVertex));
		d3dDevice->SetIndices(meshData.indexBuffer);
		
		// Create world transformation matrix for this mesh
		glm::mat4 worldGL = glm::mat4(1.0f);
		worldGL = glm::translate(worldGL, mesh->transform.position);
		worldGL = glm::rotate(worldGL, glm::radians(mesh->transform.rotation.y), glm::vec3(0,1,0));
		worldGL = glm::rotate(worldGL, glm::radians(mesh->transform.rotation.x), glm::vec3(1,0,0));
		worldGL = glm::rotate(worldGL, glm::radians(mesh->transform.rotation.z), glm::vec3(0,0,1));
		
		auto worldMtr = ToD3D(worldGL);
		d3dDevice->SetTransform(D3DTS_WORLD, &worldMtr);
		
		// Draw this mesh
		d3dDevice->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST,
			0,                      // BaseVertexIndex
			0,                      // MinIndex  
			meshData.vertexCount,   // NumVertices
			0,                      // StartIndex
			meshData.triangleCount  // PrimitiveCount
		);
	}

	d3dDevice->EndScene();
	d3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
}
