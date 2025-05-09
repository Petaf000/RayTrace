#pragma once
#define PI 3.14159265359f
#define PI2 6.28318530718f
#define RECIP_PI 0.31830988618f
#define RECIP_PI2 0.15915494f
#define LOG2 1.442695f
#define EPSILON 1e-6f
#define GAMMA_FACTOR 2.2f

inline float drand48() {
    return float(( (double)( rand() ) / ( RAND_MAX ) )); /* RAND_MAX = 32767 */
}

inline float pow2(float x) { return x * x; }
inline float pow3(float x) { return x * x * x; }
inline float pow4(float x) { return x * x * x * x; }
inline float pow5(float x) { return x * x * x * x * x; }
inline float clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
inline float saturate(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
inline float recip(float x) { return 1.f / x; }
inline float mix(float a, float b, float t) { return a * ( 1.f - t ) + b * t; /* return a + (b-a) * t; */ }
inline float step(float edge, float x) { return ( x < edge ) ? 0.f : 1.f; }
inline float smoothstep(float a, float b, float t) { if ( a >= b ) return 0.f; float x = saturate(( t - a ) / ( b - a )); return x * x * ( 3.f - 2.f * t ); }
inline float radians(float deg) { return ( deg / 180.f ) * PI; }
inline float degrees(float rad) { return ( rad / PI ) * 180.f; }

// 単位球の中のランダムな点を返す
inline Vector3 random_vector() {
    return Vector3(drand48(), drand48(), drand48());
}

inline Vector3 random_in_unit_sphere() {
    Vector3 p;
    do {
        p = 2.f * random_vector() - Vector3(1.f);
    } while ( lengthSqr(p) >= 1.f );
    return p;
}

// リニアからsRGB変換
inline Vector3 linear_to_gamma(const Vector3& v, float gammaFactor) {
    float recipGammaFactor = recip(gammaFactor);
    return Vector3(
        powf(v.getX(), recipGammaFactor),
        powf(v.getY(), recipGammaFactor),
        powf(v.getZ(), recipGammaFactor));
}

inline Vector3 gamma_to_linear(const Vector3& v, float gammaFactor) {
    return Vector3(
        powf(v.getX(), gammaFactor),
        powf(v.getY(), gammaFactor),
        powf(v.getZ(), gammaFactor));
}

inline Vector3 reflect(const Vector3& v, const Vector3& n) {
    return v - 2.f * dot(v, n) * n;
}

inline bool refract(const Vector3& v, const Vector3& n, float ni_over_nt, Vector3& refracted) {
    Vector3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.f - pow2(ni_over_nt) * ( 1.f - pow2(dt) );
    if ( D > 0.f ) {
        refracted = -ni_over_nt * ( uv - n * dt ) - n * sqrt(D);
        return true;
    }
    else {
        return false;
    }
}

inline float schlick(float cosine, float ri) {
    float r0 = pow2(( 1.f - ri ) / ( 1.f + ri ));
    return r0 + ( 1.f - r0 ) * pow5(1.f - cosine);
}

inline void get_sphere_uv(const Vector3& p, float& u, float& v) {
    float phi = atan2(p.getZ(), p.getX());
    float theta = asin(p.getY());
    u = 1.f - ( phi + PI ) / ( 2.f * PI );
    v = ( theta + PI / 2.f ) / PI;
}