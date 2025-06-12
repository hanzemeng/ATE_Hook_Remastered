#include "pch.h"
#include "framework.h"
#include "Inject_Assembly.h"
#include "MultiColorOutlineTextRenderer.h"
#include <tchar.h>
#include <windows.h>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")


std::ofstream m_logFile;

const wchar_t* m_targetWindowName = L"Muv-Luv Alternative Total Eclipse Ver 1.0.27";
const wchar_t* m_overlayClassName = L"Muv-Luv Alternative Total Eclipse Hook Remastered";

HWND m_targeHhwnd = nullptr;
HWND m_overlayHwnd = nullptr;

const int m_injectAssemblySize = 1024;
void* m_injectAssembly;

unsigned long long m_textTagPointer;

CRITICAL_SECTION m_textBufferCS;
const int m_textBufferSize = 1024;
char m_textBuffer[m_textBufferSize] = {0};
wchar_t m_wideBuffer[m_textBufferSize] = {0};

ID2D1Factory* m_d2D1FactoryPtr = nullptr;
IDWriteFactory* m_dWriteFactoryPtr = nullptr;


int m_skipOnGameProcess = 0;
int m_gameProcessGap = 0;
RECT m_lastRenderRect;
std::string m_lastRenderText = "";


const wchar_t* m_translationFileName = L"ATE_Hook_Remastered_Translation.tsv";
const char m_translationDelimiter = '\t';
const char m_translationEscapeChar = '\\';
const wchar_t m_translationNameBegin = 0x3010;
const wchar_t m_translationNameEnd = 0x3011;
const int m_translationsSize = 30000;
std::string m_translations[m_translationsSize];
int m_translationsPause[m_translationsSize];

const wchar_t* m_translationColorFileName = L"ATE_Hook_Remastered_Color.tsv";
std::unordered_map<std::wstring, D2D1_COLOR_F> m_castColors;
D2D1_COLOR_F m_defaultCastColor = {0,0,0,0};


const wchar_t* m_configFileName = L"ATE_Hook_Remastered_Config.txt";
const char m_configFileDelimiter = ':';

float m_textBoxLengthRatio = 0.7;
int m_textBoxBottomOffset = 75;
wchar_t m_fontName[32] = L"Microsoft YaHei";
int m_fontSize = 24;
float m_outlineSize = 2.5;
bool m_showTextTag = false;
bool m_showOriginalText = false;


void LoadTranslation()
{
	for (int i = 0; i < m_translationsSize; i++)
	{
		m_translations[i] = "";
		m_translationsPause[i] = 0;
	}
	std::ifstream translationFile;
	translationFile.open(m_translationFileName);
	std::string line;

	while (std::getline(translationFile, line))
	{
		size_t i = line.find_first_of(m_translationDelimiter);
		if (i == std::string::npos || i != 12)
		{
			continue;
		}
		std::string name = line.substr(7, i-7);
		std::string val = line.substr(i + 1);
		int index = std::stoi(name);

		i = val.find_first_of(m_translationDelimiter);
		if (i != val.length() - 1)
		{
			m_translationsPause[index] = std::stoi(val.substr(i + 1));
			val = val.substr(0, i);
		}
		
		m_translations[index] = val;
	}

	translationFile.close();

	translationFile.open(m_translationColorFileName);
	bool hasDefaultColor = false;
	while (std::getline(translationFile, line))
	{
		size_t i0 = line.find_first_of(m_translationDelimiter);
		if (i0 == std::string::npos)
		{
			continue;
		}
		size_t i1 = line.find_first_of(m_translationDelimiter, i0+1);
		size_t i2 = line.find_first_of(m_translationDelimiter, i1+1);
		std::string name = line.substr(0, i0);

		int c = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
		std::wstring wname(c, 0);
		MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wname[0], c);

		D2D1_COLOR_F color =
		{
			std::stof(line.substr(i0 + 1, i1 - i0 - 1)) / 255.0f, // r
			std::stof(line.substr(i1 + 1, i2 - i1 - 1)) / 255.0f, // g
			std::stof(line.substr(i2 + 1)) / 255.0f, // b
			1.0f // a
		};


		if (!hasDefaultColor)
		{
			m_defaultCastColor = color;
			hasDefaultColor = true;
		}
		else
		{
			m_castColors[wname] = color;
		}
	}

	translationFile.close();
}


void UpdateConfig()
{
	std::ifstream configFile;
	configFile.open(m_configFileName);
	std::string line;
	while (std::getline(configFile, line))
	{
		size_t i = line.find_first_of(m_configFileDelimiter);
		if (i == std::string::npos)
		{
			continue;
		}
		std::string name = line.substr(0, i);
		std::string val = line.substr(i+1);
		name.erase(name.find_last_not_of(' ')+1);
		name.erase(0, name.find_first_not_of(' '));
		val.erase(val.find_last_not_of(' ') + 1);
		val.erase(0, val.find_first_not_of(' '));

		if ("text_box_length_ratio" == name)
		{
			m_textBoxLengthRatio = std::stof(val);
		}
		else if ("text_box_bottom_offset" == name)
		{
			m_textBoxBottomOffset = std::stoi(val);
		}
		else if ("font_name" == name)
		{
			int n = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, m_fontName, 32);
			m_fontName[n] = 0;
		}
		else if ("font_size" == name)
		{
			m_fontSize = std::stoi(val);
		}
		else if ("outline_size" == name)
		{
			m_outlineSize = std::stof(val);
		}
		else if ("show_text_tag" == name)
		{
			m_showTextTag = 1 == std::stoi(val);
		}
		else if ("show_original_text" == name)
		{
			m_showOriginalText = 1 == std::stoi(val);
		}
	}

	configFile.close();
}

int Utf8CharSize(char leadChar)
{
	int c = -1;
	if ((leadChar & 0b10000000) == 0)
	{
		c = 1;
	}
	else if ((leadChar & 0b11100000) == 0b11000000)
	{
		c = 2;
	}
	else if ((leadChar & 0b11110000) == 0b11100000)
	{
		c = 3;
	}
	else if ((leadChar & 0b11111000) == 0b11110000)
	{
		c = 4;
	}
	return c;
}

#define WM_MY_TIMER (WM_USER + 1)
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_MY_TIMER: {
		EnterCriticalSection(&m_textBufferCS);
		RECT rect;
		GetWindowRect(m_targeHhwnd, &rect);
		std::string translation(m_textBuffer + 7, 5);
		if (0x00 == m_textBuffer[0])
		{
			translation = "";
		}
		bool shouldProcess = m_lastRenderRect.left != rect.left || m_lastRenderRect.right != rect.right || m_lastRenderRect.top != rect.top || m_lastRenderRect.bottom != rect.bottom
			|| m_lastRenderText != translation;
		if (!m_showTextTag && !shouldProcess)
		{
			LeaveCriticalSection(&m_textBufferCS);
			return 0;
		}
		m_lastRenderRect = rect;
		m_lastRenderText = translation;


		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		HDC screenDC = GetDC(NULL);
		HDC memDC = CreateCompatibleDC(screenDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
		HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);
		ID2D1DCRenderTarget* renderTargetPtr = nullptr;
		D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),0, 0,D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
		m_d2D1FactoryPtr->CreateDCRenderTarget(&props, &renderTargetPtr);
		RECT drawAera = { 0, 0, width, height };
		renderTargetPtr->BindDC(memDC, &drawAera);

		ID2D1SolidColorBrush* brushPtr = nullptr;
		renderTargetPtr->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brushPtr);
		ID2D1SolidColorBrush* outlineBrushPtr = nullptr;
		renderTargetPtr->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &outlineBrushPtr);
		ID2D1StrokeStyle* outlineStrokeStylePtr = nullptr;
		D2D1_STROKE_STYLE_PROPERTIES strokeProps = {};
		strokeProps.startCap = D2D1_CAP_STYLE_ROUND;
		strokeProps.endCap = D2D1_CAP_STYLE_ROUND;
		strokeProps.lineJoin = D2D1_LINE_JOIN_ROUND;
		strokeProps.miterLimit = 10.0f;
		m_d2D1FactoryPtr->CreateStrokeStyle(&strokeProps, nullptr, 0, &outlineStrokeStylePtr);
		
		IDWriteTextFormat* renderTextFormatPtr;
		m_dWriteFactoryPtr->CreateTextFormat(m_fontName, NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, m_fontSize, L"en-us", &renderTextFormatPtr);
		MultiColorOutlineTextRenderer* textRendererPtr = new MultiColorOutlineTextRenderer(m_d2D1FactoryPtr);
		
		renderTargetPtr->BeginDraw();
		renderTargetPtr->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.f));

		if (m_showTextTag)
		{
			std::string gameProcessGap = std::to_string(m_gameProcessGap);
			m_textBuffer[12] = ' ';
			memcpy(m_textBuffer + 13, &gameProcessGap[0], gameProcessGap.length() + 1);
			int c = MultiByteToWideChar(CP_UTF8, 0, m_textBuffer, 13+gameProcessGap.length(), m_wideBuffer, m_textBufferSize);
			IDWriteTextLayout* textLayoutPtr;
			m_dWriteFactoryPtr->CreateTextLayout(m_wideBuffer, c, renderTextFormatPtr, 400, 200, &textLayoutPtr);
			textLayoutPtr->SetDrawingEffect(brushPtr, DWRITE_TEXT_RANGE{ 0,(unsigned int)c });
			textLayoutPtr->Draw(nullptr, textRendererPtr, 0, 0);
			textRendererPtr->RenderAll(renderTargetPtr, 100, 100, outlineBrushPtr, m_outlineSize, outlineStrokeStylePtr);
			textLayoutPtr->Release();
		}

		if (0x00 != m_textBuffer[0])
		{
			textRendererPtr->Clear();
			translation = m_translations[std::stoi(translation)];
			int i = 0;
			std::string tempStr;
			while (i < translation.length())
			{
				if ('\\' != translation[i])
				{
					tempStr += translation[i];
					i++;
					continue;
				}

				if ('n' == translation[i + 1])
				{
					tempStr += '\n';
					i += 2;
					continue;
				}
				i++;
			}
			translation = tempStr;

			int c = MultiByteToWideChar(CP_UTF8, 0, translation.c_str(),-1, m_wideBuffer, m_textBufferSize);
			IDWriteTextLayout* textLayoutPtr;
			m_dWriteFactoryPtr->CreateTextLayout(m_wideBuffer, c, renderTextFormatPtr, m_textBoxLengthRatio * (float)width, height, &textLayoutPtr);
			textLayoutPtr->SetDrawingEffect(brushPtr, DWRITE_TEXT_RANGE{ 0, (unsigned int)c });

			std::vector<ID2D1SolidColorBrush*> brushesPtrs;
			i = 0;
			while (i<c)
			{
				if (m_wideBuffer[i] != m_translationNameBegin)
				{
					i++;
					continue;
				}
				int j = i + 1;
				while (m_wideBuffer[j] != m_translationNameEnd)
				{
					j++;
				}


				std::wstring name(m_wideBuffer + i + 1, j - i - 1);
				name.push_back(0);
				D2D1::ColorF color(m_defaultCastColor.r, m_defaultCastColor.g, m_defaultCastColor.b, m_defaultCastColor.a);
				if (m_castColors.end() != m_castColors.find(name))
				{
					color = {m_castColors[name].r, m_castColors[name].g, m_castColors[name].b, m_castColors[name].a};
				}

				ID2D1SolidColorBrush* bPtr;
				renderTargetPtr->CreateSolidColorBrush(color, &bPtr);
				brushesPtrs.push_back(bPtr);
				textLayoutPtr->SetDrawingEffect(brushesPtrs.back(), DWRITE_TEXT_RANGE{(unsigned int)i, (unsigned int)c-i});

				i = j + 1;
			}
			
			DWRITE_TEXT_METRICS metrics;
			textLayoutPtr->GetMetrics(&metrics);

			textLayoutPtr->Draw(nullptr, textRendererPtr, 0, 0);
			textRendererPtr->RenderAll(renderTargetPtr, (float)width*(1.0f-m_textBoxLengthRatio)/2.0f, height - metrics.height - m_textBoxBottomOffset, outlineBrushPtr, m_outlineSize, outlineStrokeStylePtr);
			
			for(auto b : brushesPtrs)
			{
				b->Release();
			}
			textLayoutPtr->Release();
		}
		
		renderTargetPtr->EndDraw();

		textRendererPtr->Release();
		renderTextFormatPtr->Release();
		renderTargetPtr->Release();
		brushPtr->Release();
		outlineBrushPtr->Release();
		outlineStrokeStylePtr->Release();
		
		SIZE size = { width, height };
		POINT ptSrc = { 0, 0 };
		POINT ptDest = { rect.left, rect.top };
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		UpdateLayeredWindow(hwnd, screenDC, &ptDest, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

		SelectObject(memDC, oldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(memDC);
		ReleaseDC(NULL, screenDC);
		
		LeaveCriticalSection(&m_textBufferCS);

		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void OnGameProcess()
{
	EnterCriticalSection(&m_textBufferCS);
	m_gameProcessGap++;
	if (m_skipOnGameProcess > 0)
	{
		m_skipOnGameProcess--;
		LeaveCriticalSection(&m_textBufferCS);
		return;
	}
	
	m_textBuffer[0] = 0x00;
	LeaveCriticalSection(&m_textBufferCS);
}
void OnTextProcess(unsigned long long textTagPtr)
{
	m_textTagPointer = textTagPtr;
	char* ptr = (char*)m_textTagPointer;

	if ('t' != ptr[6])
	{
		return;
	}
	
	EnterCriticalSection(&m_textBufferCS);
	m_gameProcessGap = 0;
	int i = 0;
	while (ptr[i] != 0x00 && i < m_textBufferSize)
	{
		m_textBuffer[i] = ptr[i];
		i++;
	}
	m_textBuffer[i] = 0x00;
	std::string translation(m_textBuffer + 7, 5);
	m_skipOnGameProcess = m_translationsPause[std::stoi(translation)];
	LeaveCriticalSection(&m_textBufferCS);
}


void ModifyTarget() {
	HMODULE hModule = GetModuleHandle(TEXT("te-win64vc14-release.exe"));
	m_injectAssembly = VirtualAlloc(
		NULL,
		m_injectAssemblySize,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE
	);

	// original_game_process
	LPCVOID originalGameProcess = (LPCVOID)((unsigned long long)hModule + 0x54D5E9);
	void (*onGameProcessPtr)() = OnGameProcess;
	unsigned long long temp = (unsigned long long)m_injectAssembly;
	memcpy(m_game_process_jump + 3, &temp, 8);
	temp = (unsigned long long)(onGameProcessPtr);
	memcpy(m_game_process_process + PUSH_REGISTERS_INSTRUCTIONS_SIZE + 1 + 11 + 3 - 1, &temp, 8);
	temp = (unsigned long long)originalGameProcess + 13;
	memcpy(m_game_process_process + 2*PUSH_REGISTERS_INSTRUCTIONS_SIZE + 1 + 11 + 12 + 8 + 17 + 4 - 1, &temp, 8);
	memcpy(m_injectAssembly, m_game_process_process, sizeof(m_game_process_process));


	// original_text_process
	LPCVOID originalTextProcess = (LPCVOID)((unsigned long long)hModule + 0x414cb2);
	void (*onTextProcessPtr)(unsigned long long) = OnTextProcess;
	temp = (unsigned long long)m_injectAssembly + PROCESS_INSTRUCTIONS_SIZE;
	memcpy(m_text_process_jump + 3, &temp, 8);
	temp = (unsigned long long)(onTextProcessPtr);
	memcpy(m_text_process_process + PUSH_REGISTERS_INSTRUCTIONS_SIZE + 1 + 14 + 3 - 1, &temp, 8);
	temp = (unsigned long long)originalTextProcess + 13;
	memcpy(m_text_process_process + 2 * PUSH_REGISTERS_INSTRUCTIONS_SIZE + 1 + 14 + 12 + 8 + 17 + 4 - 1, &temp, 8);
	temp = (unsigned long long)m_injectAssembly + PROCESS_INSTRUCTIONS_SIZE;
	memcpy((void*)temp, m_text_process_process, sizeof(m_text_process_process));
	
	WriteProcessMemory(GetCurrentProcess(), (LPVOID)originalGameProcess, m_game_process_jump, sizeof(m_game_process_jump), NULL);
	WriteProcessMemory(GetCurrentProcess(), (LPVOID)originalTextProcess, m_text_process_jump, sizeof(m_text_process_jump), NULL);
	if (!m_showOriginalText)
	{
		LPCVOID originalTextRender = (LPCVOID)((unsigned long long)hModule + 0x414cf2);
		WriteProcessMemory(GetCurrentProcess(), (LPVOID)originalTextRender, m_text_render, sizeof(m_text_render), NULL);
	}
}


VOID CALLBACK TimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	HWND hwnd = (HWND)lpParam;
	PostMessage(hwnd, WM_MY_TIMER, 0, 0);
}

DWORD WINAPI DllThread(LPVOID lpParam) {
	HMODULE hInstance = (HMODULE)lpParam;

	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = m_overlayClassName;
	RegisterClass(&wc);

	m_targeHhwnd = FindWindow(NULL, m_targetWindowName);
	if (!m_targeHhwnd) {
		MessageBox(NULL, L"Target window not found!", L"Error", MB_OK);
		return 1;
	}

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2D1FactoryPtr);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_dWriteFactoryPtr));

	LoadTranslation();
	UpdateConfig();
	ModifyTarget();

	InitializeCriticalSection(&m_textBufferCS);

	RECT rect;
	GetWindowRect(m_targeHhwnd, &rect);
	m_overlayHwnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		m_overlayClassName, NULL, WS_POPUP,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, hInstance, NULL);
	ShowWindow(m_overlayHwnd, SW_SHOW);


	HANDLE hTimer = NULL;
	CreateTimerQueueTimer(&hTimer, NULL, TimerCallback, m_overlayHwnd, 0, 50, WT_EXECUTEDEFAULT);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	m_d2D1FactoryPtr->Release();
	m_dWriteFactoryPtr->Release();
}

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		CreateThread(NULL, 0, DllThread, (LPVOID)hModule, 0, NULL);
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
