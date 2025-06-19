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

using namespace Renderer;

//-----------------------------------------------------------------------------
// raw mouse deltas
static float g_rawMouseX = 0.0f;
static float g_rawMouseY = 0.0f;

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

//------------------------------------------------------------------------
bool RendererDX9::SetMeshes(std::vector<std::shared_ptr<Mesh>> msh) {
	meshes = std::move(msh);
	numberOfMeshVertexes = 0; UINT numindx = 0;
	for (auto& mesh : meshes) {
		numindx += static_cast<UINT>(mesh->indices.size());
		numberOfMeshVertexes += static_cast<UINT>(mesh->vertices.size());
	}
	numIndices = numindx;

	// Vertex buffer
	if (vb) { vb->Release(); vb = nullptr; }
	if (FAILED(d3dDevice->CreateVertexBuffer(
			numberOfMeshVertexes * sizeof(DXVertex),
			0, D3DFVF_DXVERTEX, D3DPOOL_MANAGED, &vb, nullptr
		))) {
		Logger::Error("Failed to create vertex buffer.");
		return false;
	}
	
	{
		DXVertex* ptr = nullptr;
		vb->Lock(0, 0, reinterpret_cast<void**>(&ptr), 0);
		int idx = 0;
		for (auto& mesh : meshes) {
			for (auto& v : mesh->vertices) {
				ptr[idx++] = {
					v.position.x + mesh->position.x,
					v.position.y + mesh->position.y,
					v.position.z + mesh->position.z,
					v.normal.x, v.normal.y, v.normal.z,
					v.texcoord.x, v.texcoord.y
				};
			}
		}
		vb->Unlock();
	}

	// Index buffer
	if (ib) { ib->Release(); ib = nullptr; }
	if (FAILED(d3dDevice->CreateIndexBuffer(
			numIndices * sizeof(uint32_t),
			0, D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib, nullptr
		))) {
		Logger::Error("Failed to create index buffer.");
		return false;
	}
	
	{
		uint32_t* ptr = nullptr;
		ib->Lock(0, 0, reinterpret_cast<void**>(&ptr), 0);
		int idx = 0; uint32_t base = 0;
		for (auto& mesh : meshes) {
			for (auto i : mesh->indices) {
				ptr[idx++] = i + base;
			}
			base += static_cast<uint32_t>(mesh->vertices.size());
		}
		ib->Unlock();
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
	auto mtr = ToD3D(projGL); // to stop CodeWizard from complaining I changed this here ever so slightly
	d3dDevice->SetTransform(D3DTS_PROJECTION, &mtr);

	// view
	glm::mat4 viewGL = glm::lookAt(cam->transform.position, cam->lookAtPosition, glm::vec3(0, 1, 0));
	auto mtr2 = ToD3D(viewGL); // again here, a slight alteration for CodeWizard.
	d3dDevice->SetTransform(D3DTS_VIEW, &mtr2); // ahh finally. Peace at last, no errors in CodeWizard.

	// draw meshes
	d3dDevice->SetFVF(D3DFVF_DXVERTEX);
	d3dDevice->SetStreamSource(0, vb, 0, sizeof(DXVertex));
	d3dDevice->SetIndices(ib);

	// material + lighting
	D3DMATERIAL9 mat = {};
	mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = 1.0f;
	mat.Ambient.r = mat.Ambient.g = mat.Ambient.b = 0.1f;
	mat.Diffuse.a = mat.Ambient.a = 1.0f;
	d3dDevice->SetMaterial(&mat);

	D3DLIGHT9 light = {};
	light.Type      = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = light.Diffuse.g = light.Diffuse.b = 1.0f;
	light.Ambient.r = light.Ambient.g = light.Ambient.b = 1.0f;
	light.Direction = {1.0f, 1.0f, 1.0f};
	d3dDevice->SetLight(0, &light);
	d3dDevice->LightEnable(0, TRUE);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	// draw
	d3dDevice->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		0, 0,
		numberOfMeshVertexes,
		0, numIndices/3
	);

	d3dDevice->EndScene();
	d3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
}
