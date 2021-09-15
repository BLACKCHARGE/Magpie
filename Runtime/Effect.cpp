#include "pch.h"
#include "Effect.h"
#include "App.h"
#include "Utils.h"
#include <DirectXColors.h>

extern std::shared_ptr<spdlog::logger> logger;


bool Effect::InitializeFromString(std::string_view hlsl) {
	/*Renderer& renderer = App::GetInstance().GetRenderer();
	_Pass& pass = _passes.emplace_back();

	if (!pass.Initialize(this, hlsl)) {
		SPDLOG_LOGGER_ERROR(logger, "Pass1 初始化失败");
		return false;
	}
	PassDesc& desc = _passDescs.emplace_back();
	desc.inputs.push_back(0);
	desc.samplers.push_back(0);
	desc.output = 1;

	_samplers.emplace_back(renderer.GetSampler(Renderer::FilterType::LINEAR));*/

	return false;
}

bool Effect::InitializeFromFile(const wchar_t* fileName) {
	std::string psText;
	if (!Utils::ReadTextFile(fileName, psText)) {
		SPDLOG_LOGGER_ERROR(logger, fmt::format("读取着色器文件{}失败", Utils::UTF16ToUTF8(fileName)));
		return false;
	}

	return InitializeFromString(psText);
}

bool Effect::InitializeFsr() {
	Renderer& renderer = App::GetInstance().GetRenderer();
	_d3dDevice = renderer.GetD3DDevice();
	_d3dDC = renderer.GetD3DDC();
	_vertexShader = renderer.GetVSShader();

	const wchar_t* fileName = L"shaders\\FsrEasu.hlsl";
	std::string easuHlsl;
	if (!Utils::ReadTextFile(fileName, easuHlsl)) {
		SPDLOG_LOGGER_ERROR(logger, fmt::format("读取着色器文件{}失败", Utils::UTF16ToUTF8(fileName)));
		return false;
	}

	fileName = L"shaders\\FsrRcas.hlsl";
	std::string rcasHlsl;
	if (!Utils::ReadTextFile(fileName, rcasHlsl)) {
		SPDLOG_LOGGER_ERROR(logger, fmt::format("读取着色器文件{}失败", Utils::UTF16ToUTF8(fileName)));
		return false;
	}

	_passes.reserve(2);
	_Pass& easuPass = _passes.emplace_back();
	_Pass& rcasPass = _passes.emplace_back();

	if (!easuPass.Initialize(this, easuHlsl)) {
		SPDLOG_LOGGER_ERROR(logger, "easuPass 初始化失败");
		return false;
	}
	if (!rcasPass.Initialize(this, rcasHlsl)) {
		SPDLOG_LOGGER_ERROR(logger, "rcasPass 初始化失败");
		return false;
	}

	_passDescs.reserve(2);
	PassDesc& desc1 = _passDescs.emplace_back();
	desc1.inputs.push_back(0);
	desc1.samplers.push_back(0);
	desc1.output = 1;

	PassDesc& desc2 = _passDescs.emplace_back();
	desc2.inputs.push_back(1);
	desc2.samplers.push_back(0);
	desc2.output = 2;

	ID3D11SamplerState* sam = nullptr;
	if (!renderer.GetSampler(Renderer::FilterType::LINEAR, &sam)) {
		SPDLOG_LOGGER_ERROR(logger, "GetSampler 失败");
		return false;
	}
	_samplers.emplace_back(sam);

	
	for (int i = 0; i < 5; ++i) {
		_constants.emplace_back();
	}

	// 大小必须为 4 的倍数
	_constants.resize((_constants.size() + 3) / 4 * 4);

	return true;
}

bool Effect::SetConstant(int index, float value) {
	if (index < 0 || index >= _constantDescs.size()) {
		return false;
	}

	const auto& desc = _constantDescs[index];
	if (desc.type != EffectConstantType::Float) {
		return false;
	}

	if (_constants[static_cast<size_t>(index) + 2].floatVal == value) {
		return true;
	}

	// 检查是否是合法的值
	if (desc.minValue.index() == 1) {
		if (desc.includeMin) {
			if (value < std::get<float>(desc.minValue)) {
				return false;
			}
		} else {
			if (value <= std::get<float>(desc.minValue)) {
				return false;
			}
		}
	}

	if (desc.maxValue.index() == 1) {
		if (desc.includeMax) {
			if (value > std::get<float>(desc.maxValue)) {
				return false;
			}
		} else {
			if (value >= std::get<float>(desc.maxValue)) {
				return false;
			}
		}
	}

	_constants[static_cast<size_t>(index) + 2].floatVal = value;

	return true;
}

bool Effect::SetConstant(int index, int value) {
	if (index < 0 || index >= _constantDescs.size()) {
		return false;
	}

	const auto& desc = _constantDescs[index];
	if (desc.type != EffectConstantType::Int) {
		return false;
	}

	if (_constants[index].intVal == value) {
		return true;
	}

	// 检查是否是合法的值
	if (desc.minValue.index() == 2) {
		if (desc.includeMin) {
			if (value < std::get<int>(desc.minValue)) {
				return false;
			}
		} else {
			if (value <= std::get<int>(desc.minValue)) {
				return false;
			}
		}
	}

	if (desc.maxValue.index() == 2) {
		if (desc.includeMax) {
			if (value > std::get<int>(desc.maxValue)) {
				return false;
			}
		} else {
			if (value >= std::get<int>(desc.maxValue)) {
				return false;
			}
		}
	}

	_constants[index].intVal = value;

	return true;
}

SIZE Effect::CalcOutputSize(SIZE inputSize) const {
	return {};
}

bool Effect::CanSetOutputSize() const {
	return true;
}

void Effect::SetOutputSize(SIZE value) {
	
}

bool Effect::Build(ComPtr<ID3D11Texture2D> input, ComPtr<ID3D11Texture2D> output) {
	D3D11_TEXTURE2D_DESC inputDesc;
	input->GetDesc(&inputDesc);
	D3D11_TEXTURE2D_DESC outputDesc;
	output->GetDesc(&outputDesc);

	_textures.emplace_back(input);

	ComPtr<ID3D11Texture2D>& tex1 = _textures.emplace_back();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.Width = outputDesc.Width;
	desc.Height = outputDesc.Height;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	HRESULT hr = App::GetInstance().GetRenderer().GetD3DDevice()->CreateTexture2D(&desc, nullptr, &tex1);
	if (FAILED(hr)) {
		SPDLOG_LOGGER_ERROR(logger, MakeComErrorMsg("创建 Texture2D 失败", hr));
		return false;
	}

	_textures.emplace_back(output);


	_constants[0].intVal = inputDesc.Width;
	_constants[1].intVal = inputDesc.Height;
	_constants[2].intVal = outputDesc.Width;
	_constants[3].intVal = outputDesc.Height;
	_constants[4].floatVal = 0.87f;

	// 创建常量缓冲区
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = 4 * (UINT)_constants.size();
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = _constants.data();
	
	hr = _d3dDevice->CreateBuffer(&bd, &initData, &_constantBuffer);
	if (FAILED(hr)) {
		SPDLOG_LOGGER_ERROR(logger, MakeComErrorMsg("CreateBuffer 失败", hr));
		return false;
	}

	for (int i = 0; i < _passes.size(); ++i) {
		PassDesc& desc = _passDescs[i];
		if (!_passes[i].Build(desc.inputs, desc.samplers, desc.output)) {
			SPDLOG_LOGGER_ERROR(logger, fmt::format("构建 Pass{} 时出错", i + 1));
			return false;
		}
	}

	return true;
}

void Effect::Draw() {
	ID3D11Buffer* t = _constantBuffer.Get();
	_d3dDC->PSSetConstantBuffers(0, 1, &t);

	for (_Pass& pass : _passes) {
		pass.Draw();
	}
}


bool Effect::_Pass::Initialize(Effect* parent, const std::string& pixelShader) {
	Renderer& renderer = App::GetInstance().GetRenderer();
	_parent = parent;
	_d3dDC = renderer.GetD3DDC();

	ComPtr<ID3DBlob> blob = nullptr;
	if (!Utils::CompilePixelShader(pixelShader.c_str(), pixelShader.size(), &blob)) {
		return false;
	}
	
	HRESULT hr = renderer.GetD3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &_pixelShader);
	if (FAILED(hr)) {
		SPDLOG_LOGGER_ERROR(logger, MakeComErrorMsg("创建像素着色器失败", hr));
		return false;
	}

	return true;
}

bool Effect::_Pass::Build(
	const std::vector<int>& inputs,
	const std::vector<int>& samplers,
	int output
) {
	Renderer& renderer = App::GetInstance().GetRenderer();
	HRESULT hr;

	_inputs.resize(inputs.size());
	for (int i = 0; i < _inputs.size(); ++i) {
		hr = renderer.GetShaderResourceView(_parent->_textures[inputs[i]].Get(), &_inputs[i]);
		if (FAILED(hr)) {
			SPDLOG_LOGGER_ERROR(logger, MakeComErrorMsg("获取 ShaderResourceView 失败", hr));
			return false;
		}
	}

	_samplers.resize(samplers.size());
	for (int i = 0; i < _samplers.size(); ++i) {
		_samplers[i] = _parent->_samplers[samplers[i]].Get();
	}

	ComPtr<ID3D11Texture2D> outputTex = _parent->_textures[output];
	hr = App::GetInstance().GetRenderer().GetRenderTargetView(outputTex.Get(), &_outputRtv);
	if (FAILED(hr)) {
		SPDLOG_LOGGER_ERROR(logger, MakeComErrorMsg("获取 RenderTargetView 失败", hr));
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	outputTex->GetDesc(&desc);
	SIZE outputTextureSize = { (LONG)desc.Width, (LONG)desc.Height };

	_vp.Width = (float)outputTextureSize.cx;
	_vp.Height = (float)outputTextureSize.cy;
	_vp.MinDepth = 0.0f;
	_vp.MaxDepth = 1.0f;

	return true;
}

void Effect::_Pass::Draw() {
	_d3dDC->PSSetShaderResources(0, 0, nullptr);
	_d3dDC->OMSetRenderTargets(1, &_outputRtv, nullptr);
	_d3dDC->RSSetViewports(1, &_vp);
	
	_d3dDC->PSSetShader(_pixelShader.Get(), nullptr, 0);
	if (!_samplers.empty()) {
		_d3dDC->PSSetSamplers(0, (UINT)_samplers.size(), _samplers.data());
	}
	if (!_inputs.empty()) {
		_d3dDC->PSSetShaderResources(0, (UINT)_inputs.size(), _inputs.data());
	}

	_d3dDC->Draw(3, 0);
}