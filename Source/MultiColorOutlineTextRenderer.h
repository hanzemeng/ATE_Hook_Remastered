#pragma once
#include "pch.h"
#include "framework.h"
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

using Microsoft::WRL::ComPtr;
struct GlyphRunGeometry {
	ComPtr<ID2D1PathGeometry> geometry;
	ComPtr<ID2D1Brush> brush;
	float baselineOriginX;
	float baselineOriginY;
};
class MultiColorOutlineTextRenderer : public IDWriteTextRenderer {
public:
	MultiColorOutlineTextRenderer(ID2D1Factory* d2dFactory) : refCount(1), m_d2dFactory(d2dFactory) {}

	// IUnknown
	STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
		if (ppvObject == nullptr) return E_POINTER;
		if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteTextRenderer) || riid == __uuidof(IDWritePixelSnapping)) {
			*ppvObject = static_cast<IDWriteTextRenderer*>(this);
			AddRef();
			return S_OK;
		}
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&refCount); }
	STDMETHOD_(ULONG, Release)() override {
		ULONG newCount = InterlockedDecrement(&refCount);
		if (newCount == 0) delete this;
		return newCount;
	}

	STDMETHOD(IsPixelSnappingDisabled)(void*, BOOL* isDisabled) override {
		*isDisabled = FALSE;
		return S_OK;
	}
	STDMETHOD(GetCurrentTransform)(void*, DWRITE_MATRIX* transform) override {
		*transform = DWRITE_MATRIX{ 1, 0, 0, 1, 0, 0 };
		return S_OK;
	}
	STDMETHOD(GetPixelsPerDip)(void*, FLOAT* pixelsPerDip) override {
		*pixelsPerDip = 1.0f;
		return S_OK;
	}

	STDMETHOD(DrawUnderline)(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override {
		return S_OK; // Ignore underline
	}
	STDMETHOD(DrawStrikethrough)(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override {
		return S_OK; // Ignore strikethrough
	}
	STDMETHOD(DrawInlineObject)(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override {
		return S_OK; // Ignore inline objects
	}

	STDMETHOD(DrawGlyphRun)(
		void* clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		DWRITE_MEASURING_MODE measuringMode,
		const DWRITE_GLYPH_RUN* glyphRun,
		const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
		IUnknown* clientDrawingEffect) override
	{
		// clientDrawingEffect is expected to be an ID2D1Brush* for color info
		ID2D1Brush* runBrush = nullptr;
		if (clientDrawingEffect) {
			clientDrawingEffect->QueryInterface(&runBrush);
		}

		// Create geometry sink and path geometry for this glyph run
		ComPtr<ID2D1PathGeometry> pathGeometry;
		m_d2dFactory->CreatePathGeometry(&pathGeometry);

		ComPtr<ID2D1GeometrySink> sink;
		pathGeometry->Open(&sink);


		// Extract glyph run outline to geometry sink
		HRESULT hr = glyphRun->fontFace->GetGlyphRunOutline(
			glyphRun->fontEmSize,
			glyphRun->glyphIndices,
			glyphRun->glyphAdvances,
			glyphRun->glyphOffsets,
			glyphRun->glyphCount,
			glyphRun->isSideways,
			FALSE,
			sink.Get()
		);
		sink->Close();


		if (SUCCEEDED(hr)) {
			GlyphRunGeometry geomEntry;
			geomEntry.geometry = pathGeometry;
			geomEntry.brush = runBrush;
			geomEntry.baselineOriginX = baselineOriginX;
			geomEntry.baselineOriginY = baselineOriginY;


			m_geometries.push_back(std::move(geomEntry));
		}


		if (runBrush) runBrush->Release();
		return hr;
	}


	void RenderAll(ID2D1RenderTarget* pRenderTarget, float offsetX, float offsetY, ID2D1SolidColorBrush* outlineBrush, float outlineThickness, ID2D1StrokeStyle* pStrokeStyle) {
		for (const auto& entry : m_geometries) {
			// Translate to (offsetX + baselineOriginX, offsetY + baselineOriginY)
			float x = offsetX + entry.baselineOriginX;
			float y = offsetY + entry.baselineOriginY;


			pRenderTarget->SetTransform(D2D1::Matrix3x2F::Translation(x, y));
			pRenderTarget->DrawGeometry(entry.geometry.Get(), outlineBrush, outlineThickness, pStrokeStyle);
			pRenderTarget->FillGeometry(entry.geometry.Get(), entry.brush.Get());
		}
		pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	void Clear()
	{
		m_geometries.clear();
	}


private:
	LONG refCount;
	ID2D1Factory* m_d2dFactory;
	std::vector<GlyphRunGeometry> m_geometries;
};