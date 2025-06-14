// RendererDX9.cpp
#include "RendererDX9.h"
#include "../Core/Logger.h"
#include <GLFW/glfw3.h> // for timing
#include <glm/gtc/matrix_transform.hpp>

using namespace Renderer;

LRESULT CALLBACK RendererDX9::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Get our 'this' pointer
	if (msg == WM_CREATE) {
		auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
	}
	RendererDX9* self = reinterpret_cast<RendererDX9*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_SIZE && self && self->d3dDevice) {
		// update width/height and reset device
		self->width  = LOWORD(lParam);
		self->height = HIWORD(lParam);
		self->ResetDevice();
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool RendererDX9::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
	meshes = msh;
	
	// VB
	
	numberOfMeshVertexes = 0;
	UINT numindx = 0;
	for (auto mesh : meshes) {
		numindx += mesh->indices.size();
		numberOfMeshVertexes += mesh->vertices.size(); // I don't know if these are the same (upon review - I think they are)
	}
	
	numIndices = static_cast<UINT>(numindx);
	d3dDevice->CreateVertexBuffer(
		UINT(numberOfMeshVertexes * sizeof(DXVertex)),
		0, D3DFVF_DXVERTEX, D3DPOOL_MANAGED, &vb, nullptr
	);
	
	{
		DXVertex* ptr = nullptr;
		vb->Lock(0, 0, reinterpret_cast<void**>(&ptr), 0);
		
		int realindex = 0; // realindex because i is now not going to be correct when we have multiple meshes so we have to count this ourselfes I guess       HOPTOCOM_0
		
		for (auto mesh : meshes) {
			for (size_t i = 0; i < mesh->vertices.size(); ++i) {
				const auto& v = mesh->vertices[i];
				
				ptr[realindex] = { v.position.x+mesh->position.x, v.position.y+mesh->position.y, v.position.z+mesh->position.z,
						   v.normal.x,   v.normal.y,   v.normal.z,
						   v.texcoord.x, v.texcoord.y };
				
				realindex ++;
			}
		}
		
		vb->Unlock();
	}
	
	// 11) IB
	d3dDevice->CreateIndexBuffer(
		numIndices * sizeof(uint32_t),
		0, D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib, nullptr
	);
	
	{
		uint32_t* ptr = nullptr;
		ib->Lock(0, 0, reinterpret_cast<void**>(&ptr), 0);
		
		int realindex = 0; // again as 24 lines above (hope that doesn't change. Else look for the text 'HOPTOCOM_0')
		uint32_t baseVertex = 0;
		
		for (auto mesh : meshes) {
			for (size_t i = 0; i < mesh->indices.size(); ++i) {
				ptr[realindex] = mesh->indices[i]+baseVertex;
				realindex ++;
			}
			baseVertex += static_cast<uint32_t>(mesh->vertices.size());
		}
		
		ib->Unlock();
	}
	
	return true;
}

bool RendererDX9::Init(Camera *c) {
	cam = c;
	
	// 1) Create Win32 window + store 'this' in GWLP_USERDATA
	HINSTANCE hInst = GetModuleHandle(nullptr);
	WNDCLASS wc = {};
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInst;
	wc.lpszClassName = "DX9WindowClass";
	RegisterClass(&wc);

	hwnd = CreateWindow(
		"DX9WindowClass", "HL2-Style Engine - DirectX 9",
		WS_OVERLAPPEDWINDOW, 100, 100, width, height,
		nullptr, nullptr, hInst, this  // pass 'this' for WM_CREATE
	);
	if (!hwnd) {
		Logger::Error("Failed to create Win32 window.");
		return false;
	}

	// 2) GLFW for high-resolution timing
	if (!glfwInit()) {
		Logger::Error("Failed to initialize GLFW for timing.");
		// we'll survive with coarse time - adam here: I totally agree.
	}

	// 3) Create D3D9 object
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d) {
		Logger::Error("Failed to create Direct3D9 object.");
		return false;
	}

	// 4) Fill initial present parameters
	// present parameters? Really kai? You just wanted to call this pp...
	pp = {};
	pp.Windowed               = TRUE;
	pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat       = D3DFMT_A8R8G8B8;
	pp.BackBufferWidth        = width;
	pp.BackBufferHeight       = height;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;

	// probe for 4× MSAA
	D3DMULTISAMPLE_TYPE msType    = D3DMULTISAMPLE_4_SAMPLES;
	DWORD               msQuality = 0; // oh god, you really like to multiline things dontcha? It hurts my soul to look at this. (Adam here)
	if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
			pp.BackBufferFormat, pp.Windowed,
			msType, &msQuality
		)))
	{
		pp.MultiSampleType    = msType;
		pp.MultiSampleQuality = (msQuality > 0 ? msQuality - 1 : 0);
	}else { // no - I take a stand here, thou shalt not put else on a newline from the bracket, that's just wrong and you know it. (Adam here)
		Logger::Warn("4× MSAA not supported — falling back to no MSAA.");
	}

	// 5) Create device
	if (FAILED(d3d->CreateDevice( // more of this eh?
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&pp, &d3dDevice)))
	{
		Logger::Error("Failed to create Direct3D9 device.");
		return false;
	}

	// 6) Turn on MSAA & lines
	d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,  TRUE);
	d3dDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);

	// 7) Show
	ShowWindow(hwnd, SW_SHOW);
	Logger::Info("DX9 device created with MSAA.");

	// 8) Timing
	lastTime = glfwGetTime();
	
	// 12) States
	d3dDevice->SetRenderState(D3DRS_ZENABLE,  D3DZB_TRUE);
	d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	return true;
}

void RendererDX9::ResetDevice() {
	// update present params to new client size
	pp.BackBufferWidth  = width;
	pp.BackBufferHeight = height;

	// reset
	if (FAILED(d3dDevice->Reset(&pp))) {
		Logger::Error("Failed to reset D3D device on resize.");
		return;
	}

	// re-enable the fixed-function states
	d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,  TRUE);
	d3dDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);
	d3dDevice->SetRenderState(D3DRS_ZENABLE,               TRUE);
	d3dDevice->SetRenderState(D3DRS_CULLMODE,              D3DCULL_CW);
}

void RendererDX9::RenderFrame() {
	if (!d3dDevice) return;

	// Pump messages
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) exit(0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Update time & angle
	double now = glfwGetTime();
	float  dt  = float(now - lastTime);
	lastTime   = now;
	angle     += glm::radians(45.0f) * dt;

	// Clear & begin
	d3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(25,25,25));
	d3dDevice->Clear(0, nullptr,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
		D3DCOLOR_XRGB(20,20,50),
		1.0f, 0);
	d3dDevice->BeginScene();

	// Compute projection from current size
	float aspect = float(width) / float(height);
	glm::mat4 projGL = glm::perspective(
		glm::radians(60.0f), aspect, 0.1f, 100.0f
	);
	D3DMATRIX projDX = ToD3D(projGL);
	d3dDevice->SetTransform(D3DTS_PROJECTION, &projDX);

	// View
	glm::mat4 viewGL = glm::lookAt(
		cam->position, cam->lookAtPosition, glm::vec3(0,1,0) // here is the camera positioning here just like there was in opengl, go take a gander at the comment there if you want.
	);
	
	d3dDevice->SetTransform(D3DTS_VIEW, &ToD3D(viewGL)); // this rotates to the camera position and rotation (also CodeWizard doesn't like this line, something about taking the something or other of a whositwhatsit)

	// World
//	d3dDevice->SetTransform(D3DTS_WORLD,
//		&ToD3D(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0))) // we're going to undo this world rotation because we're fancy and we move the camera (and then the world round it)
//	);

	// Streams & FVF
	d3dDevice->SetFVF(D3DFVF_DXVERTEX);
	d3dDevice->SetStreamSource(0, vb, 0, sizeof(DXVertex));
	d3dDevice->SetIndices(ib);

	// Material
	D3DMATERIAL9 mat = {};
	mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = 1.0f;
	mat.Ambient .r = mat.Ambient .g = mat.Ambient .b = 0.1f;
	mat.Diffuse.a = mat.Ambient.a = 1.0f;
	d3dDevice->SetMaterial(&mat);

	// Light
	D3DLIGHT9 light = {};
	light.Type      = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = light.Diffuse.g = light.Diffuse.b = 1.0f;
	light.Ambient.r = light.Ambient.g = light.Ambient.b = 1.0f;
	light.Direction = {1.0f,1.0f,1.0f};
	d3dDevice->SetLight(0, &light);
	d3dDevice->LightEnable(0, TRUE);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	// Draw
	d3dDevice->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		0, 0,
		numberOfMeshVertexes,
		0, numIndices/3
	);

	// Present
	d3dDevice->EndScene();
	d3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
}
