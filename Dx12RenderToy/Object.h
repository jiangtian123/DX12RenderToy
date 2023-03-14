#ifndef OBJECT
#define OBJECT_H
#include <DirectXMath.h>
using namespace DirectX;
struct Object
{
	XMFLOAT3 position;
	XMFLOAT3 color;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
};

#endif