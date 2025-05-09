#pragma once

struct HitRec {
	float t; // ray parameter
	float u; // texture coordinateX
	float v; // texture coordinateY
	Vector3 p; // hit point
	Vector3 n; // normal
	MaterialPtr mat; // material
};