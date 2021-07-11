#pragma once
#include "pch.h"
#include <SimpleDrawTransform.h>
#include "EffectDefines.h"


// Adaptive Sharpen Pass 2 ��Ҫ curveHeight ����
// curveHeight Խ���񻯳̶�Խ��
// curveHeight ��ȡֵ�� 0.3~2 ֮�䣬Ĭ��ֵΪ 0.3
class AdaptiveSharpenPass2Transform : public SimpleDrawTransform<> {
private:
    AdaptiveSharpenPass2Transform(): SimpleDrawTransform<>(GUID_MAGPIE_ADAPTIVE_SHARPEN_PASS2_SHADER) {}
public:
    static HRESULT Create(_In_ ID2D1EffectContext* d2dEC, _Outptr_ AdaptiveSharpenPass2Transform** ppOutput) {
        *ppOutput = nullptr;

        HRESULT hr = LoadShader(
            d2dEC,
            MAGPIE_ADAPTIVE_SHARPEN_PASS2_SHADER,
            GUID_MAGPIE_ADAPTIVE_SHARPEN_PASS2_SHADER
        );
        if (FAILED(hr)) {
            return hr;
        }

        *ppOutput = new AdaptiveSharpenPass2Transform();

        return S_OK;
    }

    void SetCurveHeight(float value) {
        assert(value > 0);
        _curveHeight = value;
    }

    float GetCurveHeight() {
        return _curveHeight;
    }
protected:
    void _SetShaderContantBuffer(const SIZE& srcSize) override {
        struct {
            INT32 srcWidth;
            INT32 srcHeight;
            FLOAT curveHeight;
        } shaderConstants {
            srcSize.cx,
            srcSize.cy,
            _curveHeight
        };

        _drawInfo->SetPixelShaderConstantBuffer((BYTE*)&shaderConstants, sizeof(shaderConstants));
    }

private:
    float _curveHeight = 0.3f;
};