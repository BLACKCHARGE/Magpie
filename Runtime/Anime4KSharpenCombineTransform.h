#pragma once
#include "pch.h"
#include "SimpleDrawTransform.h"


class Anime4KSharpenCombineTransform : public SimpleDrawTransform {
private:
    Anime4KSharpenCombineTransform() : SimpleDrawTransform(GUID_MAGPIE_ANIME4K_SHARPEN_COMBINE_SHADER){}

public:
    static HRESULT Create(_In_ ID2D1EffectContext* d2dEC, _Outptr_ Anime4KSharpenCombineTransform** ppOutput) {
        *ppOutput = nullptr;

        HRESULT hr = DrawTransformBase::LoadShader(
            d2dEC, 
            MAGPIE_ANIME4K_SHARPEN_COMBINE_SHADER, 
            GUID_MAGPIE_ANIME4K_SHARPEN_COMBINE_SHADER
        );
        if (FAILED(hr)) {
            return hr;
        }

        *ppOutput = new Anime4KSharpenCombineTransform();

        return S_OK;
    }

    // ID2D1TransformNode Methods:
    IFACEMETHODIMP_(UINT32) GetInputCount() const override {
        return 2;
    }

    IFACEMETHODIMP MapInputRectsToOutputRect(
        _In_reads_(inputRectCount) const D2D1_RECT_L* pInputRects,
        _In_reads_(inputRectCount) const D2D1_RECT_L* pInputOpaqueSubRects,
        UINT32 inputRectCount,
        _Out_ D2D1_RECT_L* pOutputRect,
        _Out_ D2D1_RECT_L* pOutputOpaqueSubRect
    ) override {
        if (inputRectCount != 2) {
            return E_INVALIDARG;
        }
        if (pInputRects[0].right - pInputRects[0].left != pInputRects[1].right - pInputRects[1].left
            || pInputRects[0].bottom - pInputRects[0].top != pInputRects[1].bottom - pInputRects[1].top
            ) {
            return E_INVALIDARG;
        }

        _inputRect = pInputRects[0];
        *pOutputRect = {
            0,
            0,
            _inputRect.right * 2,
            _inputRect.bottom * 2,
        };
        *pOutputOpaqueSubRect = {};

        struct {
            INT32 width;
            INT32 height;
            FLOAT curveHeight;
        } _shaderConstants = {
            _inputRect.right,
            _inputRect.bottom,
            _curveHeight
        };
        _drawInfo->SetPixelShaderConstantBuffer((BYTE*)&_shaderConstants, sizeof(_shaderConstants));

        return S_OK;
    }

    IFACEMETHODIMP MapOutputRectToInputRects(
        _In_ const D2D1_RECT_L* pOutputRect,
        _Out_writes_(inputRectCount) D2D1_RECT_L* pInputRects,
        UINT32 inputRectCount
    ) const override {
        if (inputRectCount != 2) {
            return E_INVALIDARG;
        }

        // The input needed for the transform is the same as the visible output.
        pInputRects[0] = _inputRect;
        pInputRects[1] = _inputRect;

        return S_OK;
    }

    IFACEMETHODIMP MapInvalidRect(
        UINT32 inputIndex,
        D2D1_RECT_L invalidInputRect,
        _Out_ D2D1_RECT_L* pInvalidOutputRect
    ) const override {
        // This transform is designed to only accept one input.
        if (inputIndex >= 2) {
            return E_INVALIDARG;
        }

        // If part of the transform's input is invalid, mark the corresponding
        // output region as invalid. 
        *pInvalidOutputRect = D2D1::RectL(LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX);

        return S_OK;
    }

    void SetCurveHeight(float value) {
        assert(value >= 0);
        _curveHeight = value;
    }

    float GetCurveHeight() {
        return _curveHeight;
    }

private:
    float _curveHeight = 0;
};