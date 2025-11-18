#pragma once
#ifdef _WINDOWS_

struct SQUARE {
	RECT Rect;


	SQUARE(LONG width, LONG height) {
		Rect.left = 0;
		Rect.top = 0;
		Rect.right = width;
		Rect.bottom = height;
	}
	SQUARE(RECT rect) :Rect(rect) {};


	LONG GetWidth() const { return Rect.right - Rect.left; }
	LONG GetHeight() const { return Rect.bottom - Rect.top; }


#if _MSC_VER
	__declspec( property( get = GetWidth ) ) LONG Width;
	__declspec( property( get = GetHeight ) ) LONG Height;
#endif // _MSC_VER
};


struct Time {
	LARGE_INTEGER Qpf;
	LARGE_INTEGER Qpc;
	LONGLONG LastTime;


	Time() {
		QueryPerformanceFrequency(&Qpf);
		QueryPerformanceCounter(&Qpc);
		LastTime = Qpc.QuadPart;
	}


	float GetDeltaSec() {
		QueryPerformanceCounter(&Qpc);
		return (float)( Qpc.QuadPart - LastTime ) / Qpf.QuadPart;
	}




#if _MSC_VER
	// 前フレームからの経過時間(Sec)
	__declspec( property( get = GetDeltaSec ) ) float DeltaTime;


#endif // _MSC_VER
};

#endif // WINDOWS_API

