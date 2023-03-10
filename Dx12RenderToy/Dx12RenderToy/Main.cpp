#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
//添加WTL支持 方便使用COM
#include <wrl.h>
using namespace Microsoft;
using namespace Microsoft::WRL;
#include <dxgi1_6.h>
//for d3d12
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
//
#include"Object.h"
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE   _T("DirectX12 Trigger Sample")

#define GRS_THROW_IF_FAILED(hr) if (FAILED(hr)){ throw CGRSCOMException(hr); }

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	const UINT nFrameBackBuffferCount = 3U;

	int iWidth = 1024;
	int iHeight = 768;
	UINT nFrameIndex = 0;
	UINT nFrame = 0;
	
	UINT nDXGIFactoryFlags = 0u;
	UINT nRTVDescriptorSize = 0u;

	HWND hWnd = nullptr;
	MSG msg = {};

	float fAspectRatio = 3.0f;
	D3D12_VERTEX_BUFFER_VIEW stVertexBufferView = {};
	UINT64 n64FenceValue = 0ui64;
	HANDLE hFenceEvent = nullptr;
	D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight) };

	D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>				pIAdapter;
	ComPtr<ID3D12Device4>				pID3D12Device;
	ComPtr<ID3D12CommandQueue>			pICMDQueue;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBuffferCount];
	ComPtr<ID3D12RootSignature>			pIRootSignature;
	ComPtr<ID3D12PipelineState>			pIPipelineState;

	ComPtr<ID3D12CommandAllocator>		pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pICMDList;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Fence>					pIFence;

	//创建窗口
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_GLOBALCLASS;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//防止无聊的背景重绘
	wcex.lpszClassName = GRS_WND_CLASS_NAME;
	RegisterClassEx(&wcex);
	//创建DXGI
	CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5));
	//创建设备
	//循环寻找设备
	for (UINT adapterIndex = 0
		; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(adapterIndex, &pIAdapter)
		; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc = {};
		pIAdapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{//软件虚拟适配器，跳过
			continue;
		}
		//检查适配器对D3D支持的兼容级别，这里直接要求支持12.1的能力，注意返回接口的那个参数被置为了nullptr，这样
		//就不会实际创建一个设备了，也不用我们啰嗦的再调用release来释放接口。这也是一个重要的技巧，请记住！
		if (SUCCEEDED(D3D12CreateDevice(pIAdapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}
	D3D12CreateDevice(pIAdapter.Get(),D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3D12Device));

}
