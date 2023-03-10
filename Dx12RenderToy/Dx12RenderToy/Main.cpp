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
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "..\WindowsCommons\d3dx12.h"
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
	ComPtr<ID3D12Device4>				pID3DDevice;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBuffferCount];
	ComPtr<ID3D12RootSignature>			pIRootSignature;
	ComPtr<ID3D12PipelineState>			pIPipelineState;

	ComPtr<ID3D12CommandQueue>			pICMDQueue;
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
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();
	}
#endif
	//创建硬件设备
	HRESULT hadrdwareResult = D3D12CreateDevice(pIAdapter.Get(),D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDevice));
	if (FAILED(hadrdwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		pIDXGIFactory5->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));
		D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDevice));
	}
	//创建Fence
	pID3DDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&pIFence));


	//检测对MSAA质量级别的支持
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLecels;
	//msQualityLecels.Format = 
	//创建命令队列
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	//------------ D3D12_COMMAND_LIST_TYPE_DIRECT 意味着创建的Queue可以执行GPU的所有命令 -------------
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	pID3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pICMDQueue));
	//创建交换链
	//=======================================================
	// D3D12中创建一个交换链时，需要指定一个命令队列
	// 最终呈现画面前，交换链需要确定绘制操作是否完全完成了，需要队列Flush一下。
	//=======================================================
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = nFrameBackBuffferCount;
	swapChainDesc.Width = iWidth;
	swapChainDesc.Height = iHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	//----- 多重采样级别 ------------------------------
	swapChainDesc.SampleDesc.Count = 1;
	pIDXGIFactory5->CreateSwapChainForHwnd(pICMDQueue.Get(), hWnd,&swapChainDesc,nullptr,nullptr,&pISwapChain1);
	pISwapChain1.As(&pISwapChain3);
	nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
	//创建描述符堆和描述符
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = nFrameBackBuffferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	pID3DDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&pIRTVHeap));
	//获取每个RTV的实际大小
	nRTVDescriptorSize = pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//创建描述符
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
	for (size_t i = 0; i < nFrameBackBuffferCount; i++)
	{
		pISwapChain3->GetBuffer(i,IID_PPV_ARGS(&pIARenderTargets[i]));
		pID3DDevice->CreateRenderTargetView(pIARenderTargets[i].Get(),nullptr,rtvHeapHandle);
		rtvHeapHandle.Offset(1, nRTVDescriptorSize);
	}
}
