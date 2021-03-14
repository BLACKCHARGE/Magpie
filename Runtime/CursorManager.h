#pragma once
#include "pch.h"
#include "Utils.h"


class CursorManager {
public:
    CursorManager(
        HINSTANCE hInstance,
        IWICImagingFactory2* wicImgFactory,
        ID2D1DeviceContext* d2dDC,
        const D2D1_RECT_F& srcRect,
        const D2D1_RECT_F& destRect,
        bool noDisturb = false
    ) : _hInstance(hInstance), _wicImgFactory(wicImgFactory), _d2dDC(d2dDC),
        _srcRect(srcRect), _destRect(destRect), _noDistrub(noDisturb) {
        _cursorSize.cx = GetSystemMetrics(SM_CXCURSOR);
        _cursorSize.cy = GetSystemMetrics(SM_CYCURSOR);

        if (!noDisturb) {
            // 保存替换之前的 arrow 光标图像
            ComPtr<ID2D1Bitmap> arrowImg = _CursorToD2DBitmap(LoadCursor(NULL, IDC_ARROW));
            ComPtr<ID2D1Bitmap> handImg = _CursorToD2DBitmap(LoadCursor(NULL, IDC_HAND));
            ComPtr<ID2D1Bitmap> appStartingImg = _CursorToD2DBitmap(LoadCursor(NULL, IDC_APPSTARTING));

            Debug::ThrowIfWin32Failed(
                SetSystemCursor(_CreateTransparentCursor(), OCR_NORMAL),
                L"设置 OCR_NORMAL 失败"
            );
            Debug::ThrowIfWin32Failed(
                SetSystemCursor(_CreateTransparentCursor(), OCR_HAND),
                L"设置 OCR_HAND 失败"
            );
            Debug::ThrowIfWin32Failed(
                SetSystemCursor(_CreateTransparentCursor(), OCR_APPSTARTING),
                L"设置 OCR_APPSTARTING 失败"
            );
            
            _hCursorArrow = LoadCursor(NULL, IDC_ARROW);
            _cursorMap.emplace(_hCursorArrow, arrowImg);
            _cursorMap.emplace(LoadCursor(NULL, IDC_HAND), handImg);
            _cursorMap.emplace(LoadCursor(NULL, IDC_APPSTARTING), appStartingImg);

            // 限制鼠标在窗口内
            RECT r{ lroundf(srcRect.left), lroundf(srcRect.top), lroundf(srcRect.right), lroundf(srcRect.bottom) };
            Debug::ThrowIfWin32Failed(ClipCursor(&r), L"ClipCursor 失败");
            
            // 设置鼠标移动速度
            Debug::ThrowIfWin32Failed(
                SystemParametersInfo(SPI_GETMOUSESPEED, 0, &_cursorSpeed, 0),
                L"获取鼠标速度失败"
            );

            float scale = float(destRect.right - destRect.left) / (srcRect.right - srcRect.left);
            long newSpeed = std::clamp(lroundf(_cursorSpeed / scale), 1L, 20L);
            Debug::ThrowIfWin32Failed(
                SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(intptr_t)newSpeed, 0),
                L"设置鼠标速度失败"
            );
        }
	}

	CursorManager(const CursorManager&) = delete;
	CursorManager(CursorManager&&) = delete;

    ~CursorManager() {
        if (!_noDistrub) {
            ClipCursor(nullptr);

            SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(intptr_t)_cursorSpeed, 0);

            // 还原系统光标
            SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
        }
    }

	void DrawCursor() {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        Debug::ThrowIfWin32Failed(
            GetCursorInfo(&ci),
            L"GetCursorInfo 失败"
        );
        
        if (ci.hCursor == NULL) {
            return;
        }
        
        // 映射坐标
        float factor = (_destRect.right - _destRect.left) / (_srcRect.right - _srcRect.left);
        D2D1_POINT_2F targetScreenPos{
            ((FLOAT)ci.ptScreenPos.x - _srcRect.left) * factor + _destRect.left,
            ((FLOAT)ci.ptScreenPos.y - _srcRect.top) * factor + _destRect.top
        };
        // 鼠标坐标为整数，否则会出现模糊
        targetScreenPos.x = roundf(targetScreenPos.x);
        targetScreenPos.y = roundf(targetScreenPos.y);


        ICONINFO ii{};
        Debug::ThrowIfWin32Failed(GetIconInfo(ci.hCursor, &ii), L"GetIconInfo 失败");
        DeleteBitmap(ii.hbmColor);
        DeleteBitmap(ii.hbmMask);

        D2D1_RECT_F cursorRect{
            targetScreenPos.x - ii.xHotspot,
            targetScreenPos.y - ii.yHotspot,
            targetScreenPos.x + _cursorSize.cx - ii.xHotspot,
            targetScreenPos.y + _cursorSize.cy - ii.yHotspot
        };
        
        auto it = _cursorMap.find(ci.hCursor);
        if (it != _cursorMap.end()) {
            _d2dDC->DrawBitmap(it->second.Get(), &cursorRect);
        } else {
            try {
                ComPtr<ID2D1Bitmap> cursorBmp = _CursorToD2DBitmap(ci.hCursor);
                _d2dDC->DrawBitmap(cursorBmp.Get(), &cursorRect);

                // 添加进表
                _cursorMap[ci.hCursor] = cursorBmp;
            } catch (...) {
                // 如果出错，只绘制普通光标
                Debug::WriteLine(L"_CursorToD2DBitmap 出错");

                it = _cursorMap.find(_hCursorArrow);
                if (it == _cursorMap.end()) {
                    return;
                }

                cursorRect = {
                    targetScreenPos.x,
                    targetScreenPos.y,
                    targetScreenPos.x + _cursorSize.cx,
                    targetScreenPos.y + _cursorSize.cy
                };
                _d2dDC->DrawBitmap(it->second.Get(), &cursorRect);
            }
        }
	}

    void AddHookCursor(HCURSOR hTptCursor, HCURSOR hCursor) {
        assert(hTptCursor != NULL && hCursor != NULL);
        _cursorMap[hTptCursor] = _CursorToD2DBitmap(hCursor);
    }
private:
    HCURSOR _CreateTransparentCursor() {
        int len = _cursorSize.cx * _cursorSize.cy;
        BYTE* andPlane = new BYTE[len];
        memset(andPlane, 0xff, len);
        BYTE* xorPlane = new BYTE[len]{};

        HCURSOR result = CreateCursor(_hInstance, 0, 0, _cursorSize.cx, _cursorSize.cy, andPlane, xorPlane);
        Debug::ThrowIfWin32Failed(result, L"创建透明鼠标失败");

        delete[] andPlane;
        delete[] xorPlane;
        return result;
    }

    ComPtr<ID2D1Bitmap> _CursorToD2DBitmap(HCURSOR hCursor) {
        assert(hCursor != NULL);

        ComPtr<IWICBitmap> wicCursor = nullptr;
        ComPtr<IWICFormatConverter> wicFormatConverter = nullptr;
        ComPtr<ID2D1Bitmap> d2dBmpCursor = nullptr;

        Debug::ThrowIfComFailed(
            _wicImgFactory->CreateBitmapFromHICON(hCursor, &wicCursor),
            L"创建鼠标图像位图失败"
        );
        Debug::ThrowIfComFailed(
            _wicImgFactory->CreateFormatConverter(&wicFormatConverter),
            L"CreateFormatConverter 失败"
        );
        Debug::ThrowIfComFailed(
            wicFormatConverter->Initialize(
                wicCursor.Get(),
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                NULL,
                0.f,
                WICBitmapPaletteTypeMedianCut
            ),
            L"IWICFormatConverter 初始化失败"
        );
        Debug::ThrowIfComFailed(
            _d2dDC->CreateBitmapFromWicBitmap(wicFormatConverter.Get(), &d2dBmpCursor),
            L"CreateBitmapFromWicBitmap 失败"
        );

        return d2dBmpCursor;
    }

    HINSTANCE _hInstance;
    IWICImagingFactory2* _wicImgFactory;
    ID2D1DeviceContext* _d2dDC;

    HCURSOR _hCursorArrow{};
    std::map<HCURSOR, ComPtr<ID2D1Bitmap>> _cursorMap;

    SIZE _cursorSize{};

    D2D1_RECT_F _srcRect;
    D2D1_RECT_F _destRect;

    INT _cursorSpeed = 0;

    bool _noDistrub = false;
};
