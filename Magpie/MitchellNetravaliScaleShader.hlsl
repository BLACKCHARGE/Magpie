#define D2D_INPUT_COUNT 1
#define D2D_INPUT0_COMPLEX
#include "d2d1effecthelpers.hlsli"

cbuffer constants : register(b0) {
    int2 srcSize : packoffset(c0.x);
    int2 destSize : packoffset(c0.z);
};

float weight(float x) {
	float ax = abs(x);
	// Mitchel-Netravali coefficients.
	// Best psychovisual result.
	const float B = 1.0 / 3.0;
	const float C = 1.0 / 3.0;

	// Sharper version.
	// May look better in some cases.
	//const float B = 0.0;
	//const float C = 0.75;

	if (ax < 1.0) {
		return
			(
				pow(x, 2.0) * ((12.0 - 9.0 * B - 6.0 * C) * ax + (-18.0 + 12.0 * B + 6.0 * C)) +
				(6.0 - 2.0 * B)
				) / 6.0;
	} else if ((ax >= 1.0) && (ax < 2.0)) {
		return
			(
				pow(x, 2.0) * ((-B - 6.0 * C) * ax + (6.0 * B + 30.0 * C)) +
				(-12.0 * B - 48.0 * C) * ax + (8.0 * B + 24.0 * C)
				) / 6.0;
	} else {
		return 0.0;
	}
}

float4 weight4(float x) {
	return float4(
		weight(x - 2.0),
		weight(x - 1.0),
		weight(x),
		weight(x + 1.0));
}

float3 pixel(float xpos, float ypos) {
	float4 coord = D2DGetInputCoordinate(0);
	return D2DSampleInput(0, float2(xpos * srcSize.x * coord.z, ypos * srcSize.y * coord.w)).rgb;
}

float3 line_run(float ypos, float4 xpos, float4 linetaps) {
	return
		pixel(xpos.r, ypos) * linetaps.r +
		pixel(xpos.g, ypos) * linetaps.g +
		pixel(xpos.b, ypos) * linetaps.b +
		pixel(xpos.a, ypos) * linetaps.a;
}

D2D_PS_ENTRY(main) {
    float4 coord = D2DGetInputCoordinate(0);

    float2 texCoord = coord.xy / coord.zw / destSize;

    float2 stepxy = 1.0 / destSize;
    float2 pos = texCoord + stepxy * 0.5;
    float2 f = frac(pos / stepxy);

	float4 linetaps = weight4(1.0 - f.x);
	float4 columntaps = weight4(1.0 - f.y);

	//make sure all taps added together is exactly 1.0, otherwise some (very small) distortion can occur
	linetaps /= linetaps.r + linetaps.g + linetaps.b + linetaps.a;
	columntaps /= columntaps.r + columntaps.g + columntaps.b + columntaps.a;

	float2 xystart = (-1.5 - f) * stepxy + pos;
	float4 xpos = float4(xystart.x, xystart.x + stepxy.x, xystart.x + stepxy.x * 2.0, xystart.x + stepxy.x * 3.0);


	// final sum and weight normalization
	float4 final = float4(line_run(xystart.y, xpos, linetaps) * columntaps.r +
		line_run(xystart.y + stepxy.y, xpos, linetaps) * columntaps.g +
		line_run(xystart.y + stepxy.y * 2.0, xpos, linetaps) * columntaps.b +
		line_run(xystart.y + stepxy.y * 3.0, xpos, linetaps) * columntaps.a, 1);
	return final;
}