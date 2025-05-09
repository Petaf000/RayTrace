#pragma once

struct HitRec {
	float t; // ray parameter
	Vector3 p; // hit point
	Vector3 n; // normal
	float u; // texture coordinateX
	float v; // texture coordinateY
	MaterialPtr mat; // material
};