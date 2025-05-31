#include "pch.h"
#include "framework.h"
#include "Inject_Assembly.h"
#include <tchar.h>
#include <windows.h>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")


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

FontFamily* m_textFontFamilyPtr;
Font* m_textFontPtr;
HFONT m_textHFont = nullptr;

struct RenderWordData
{
	wchar_t utf16Bytes[3];
	PointF position;
	ARGB color;
};

RenderWordData m_textRenderBuffer[m_textBufferSize];
int m_skipOnGameProcess = 0;
RECT m_lastRenderRect;
std::string m_lastRenderText = "";


const wchar_t* m_translationFileName = L"ATE_Hook_Remastered_Translation.tsv";
const char m_translationDelimiter = '\t';
const char m_translationEscapeChar = '\\';
const char m_translationNameBegin[] = { 0xe3, 0x80, 0x90 };
const char m_translationNameEnd[] = { 0xe3, 0x80, 0x91 };
const int m_translationsSize = 30000;
std::string m_translations[m_translationsSize];
int m_translationsPause[m_translationsSize];

const wchar_t* m_translationColorFileName = L"ATE_Hook_Remastered_Color.tsv";
std::unordered_map<std::string, int> m_castColors;
int m_defaultCastColor = -1;


const wchar_t* m_configFileName = L"ATE_Hook_Remastered_Config.txt";
const char m_configFileDelimiter = ':';

float m_textBoxLengthRatio = 0.7;
int m_textBoxBottomOffset = 75;
wchar_t m_fontName[32] = L"Microsoft YaHei";
int m_fontSize = 24;
int m_outlineSize = 1;
int m_linePadding = 5;

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

		int color = 0xFF000000;

		color |= std::stoi(line.substr(i0 + 1, i1 - i0 - 1)) << 16; // r
		color |= std::stoi(line.substr(i1 + 1, i2 - i1 - 1)) << 8; // g
		color |= std::stoi(line.substr(i2 + 1)) << 0; // b

		if (-1 == m_defaultCastColor)
		{
			m_defaultCastColor = color;
		}
		else
		{
			m_castColors[name] = color;
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
			m_outlineSize = std::stoi(val);
		}
		else if ("line_padding" == name)
		{
			m_linePadding = std::stoi(val);
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
		if (!shouldProcess)
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

		Graphics graphics(memDC);
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);
		graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics.Clear(Color(0, 0, 0, 0));
		
		if (0x00 != m_textBuffer[0])
		{
			SolidBrush brush(Color(255, 255, 255, 255));
			if (m_showTextTag)
			{
				int c = MultiByteToWideChar(CP_UTF8, 0, m_textBuffer, 12, m_wideBuffer, m_textBufferSize);;
				graphics.DrawString(m_wideBuffer, c, m_textFontPtr, PointF(20.0f, 50.0f), &brush);
			}

			
			translation = m_translations[std::stoi(translation)];

			int i = 0;
			int ri = 0;
			int rowLength = 0;
			int rowHeight = 0;
			int totalLength = m_textBoxLengthRatio * (float)width;
			int totalHeight = 0;
			int color = 0xFFFFFFFF;
			while (i < translation.length())
			{
				int c = Utf8CharSize(translation[i]);
				if (c <= 0)
				{
					i++;
					continue;
				}

				if (m_translationEscapeChar == translation[i])
				{
					if ('n' == translation[i + 1])
					{
						if (0 == rowHeight)
						{
							totalHeight += m_fontSize+m_linePadding;
						}
						else
						{
							totalHeight += rowHeight;
						}
						rowLength = 0;
						rowHeight = 0;
					}
					i += 2;
					continue;
				}
				if (0 == strncmp(&translation[i], m_translationNameBegin, 3))
				{
					int j = i + 3;
					while (j < translation.length() && 0 != strncmp(&translation[j], m_translationNameEnd, 3))
					{
						j++;
					}
					if (j < translation.length())
					{
						auto it = m_castColors.find(translation.substr(i + 3, j - (i + 3)));
						if (it == m_castColors.end())
						{
							color = m_defaultCastColor;
						}
						else
						{
							color = it->second;
						}
					}
				}

				int wc = MultiByteToWideChar(CP_UTF8, 0, &translation[i], c, m_textRenderBuffer[ri].utf16Bytes, 3);
				m_textRenderBuffer[ri].utf16Bytes[wc] = 0;
				m_textRenderBuffer[ri].color = color;

				if (nullptr == m_textHFont)
				{
					LOGFONTW logFont;
					m_textFontPtr->GetLogFontW(&graphics, &logFont);
					m_textHFont = CreateFontIndirectW(&logFont);
				}
				SelectObject(memDC, m_textHFont);

				UINT ch = m_textRenderBuffer[ri].utf16Bytes[0];
				if (1 != wc)
				{
					ch = m_textRenderBuffer[ri].utf16Bytes[0] << 16 | m_textRenderBuffer[ri].utf16Bytes[1];
				}

				GLYPHMETRICS gm;
				MAT2 mat = { {0,1}, {0,0}, {0,0}, {0,1} };
				GetGlyphOutlineW(
					memDC,
					ch,
					GGO_METRICS,
					&gm,
					0, NULL,
					&mat
				);

				RectF size (0,0, gm.gmCellIncX, gm.gmBlackBoxY+m_linePadding);

				if (rowLength + size.Width > totalLength)
				{
					totalHeight += rowHeight;
					rowLength = 0;
					rowHeight = 0;
				}

				m_textRenderBuffer[ri].position.X = rowLength;
				m_textRenderBuffer[ri].position.Y = totalHeight;
				rowLength += size.Width;
				rowHeight = rowHeight > size.Height ? rowHeight : size.Height;

				ri++;
				i += c;
			}

			PointF start((width - totalLength) / 2, height - totalHeight - m_textBoxBottomOffset);
			
			Pen outlinePen(0xFF000000, m_outlineSize);
			outlinePen.SetLineJoin(LineJoinRound);
			//outlinePen.SetAlignment(PenAlignmentInset);
			GraphicsPath path;
			StringFormat format;
			format.SetFormatFlags(StringFormatFlagsNoClip);
			i = 0;
			while (i < ri)
			{
				path.AddString(m_textRenderBuffer[i].utf16Bytes, -1, m_textFontFamilyPtr, m_textFontPtr->GetStyle(), m_textFontPtr->GetSize(), m_textRenderBuffer[i].position + start, &format);
				i++;
			}
			graphics.DrawPath(&outlinePen, &path);
			brush.SetColor(0xFF000000);
			//graphics.FillPath(&brush, &path);

			i = 0;
			while (i < ri)
			{
				brush.SetColor(m_textRenderBuffer[i].color);
				graphics.DrawString(m_textRenderBuffer[i].utf16Bytes, -1, m_textFontPtr, m_textRenderBuffer[i].position + start, &brush);
				i++;
			}
		}
		LeaveCriticalSection(&m_textBufferCS);


		SIZE size = { width, height };
		POINT ptSrc = { 0, 0 };
		POINT ptDest = { rect.left, rect.top };
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		UpdateLayeredWindow(hwnd, screenDC, &ptDest, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

		SelectObject(memDC, oldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(memDC);
		ReleaseDC(NULL, screenDC);

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
	
	if (m_skipOnGameProcess > 0)
	{
		m_skipOnGameProcess--;
		LeaveCriticalSection(&m_textBufferCS);
		return;
	}
	
	//m_skipOnGameProcess++;
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
	int i = 0;
	while (ptr[i] != 0x00 && i < m_textBufferSize)
	{
		m_textBuffer[i] = ptr[i];
		i++;
	}
	m_textBuffer[i] = 0x00;
	std::string translation(m_textBuffer + 7, 5);
	m_skipOnGameProcess = m_translationsPause[std::stoi(translation)];
	//m_skipOnGameProcess = 0;
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

	ULONG_PTR gdi;
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdi, &gdiplusStartupInput, nullptr);
	m_logFile.open("test");

	LoadTranslation();
	UpdateConfig();
	ModifyTarget();

	InitializeCriticalSection(&m_textBufferCS);

	m_textFontFamilyPtr = new FontFamily(m_fontName);
	m_textFontPtr = new Font(m_textFontFamilyPtr, m_fontSize, FontStyleBold, UnitPixel);
	/*
	m_textHFont = CreateFontW(m_fontSize, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		m_fontName);
		*/

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
	m_logFile.close();
	GdiplusShutdown(gdi);
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
