#ifdef OML_WASM
WASM_IMPORT("env", "sinf")
extern float _oml_sinf(float);

WASM_IMPORT("env", "cosf")
extern float _oml_cosf(float);

WASM_IMPORT("env", "atan2f")
extern float _oml_atan2f(float, float);

WASM_IMPORT("env", "sqrtf")
extern float _oml_sqrtf(float);
#else
#include <math.h>
#endif

unsigned int OmlMath_Hash(unsigned int seed, unsigned int i,
                          unsigned int salt) {
    unsigned int h = seed ^ (i * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

float OmlMath_HashToUnit(unsigned int h) {
    return (h >> 8) * (1.0f / 16777216.0f);
}

float OmlMath_Lerp(float lo, float hi, float unit) {
    return (hi - lo) * unit + lo;
}

float minFloat(float a, float b) {
    return (a < b) ? a : b;
}

float maxFloat(float a, float b) {
    return (a > b) ? a : b;
}

float OmlMath_Sinf(float f) {
#ifdef OML_WASM
    return _oml_sinf(f);
#else
    return sinf(f);
#endif
}

float OmlMath_Cosf(float f) {
#ifdef OML_WASM
    return _oml_cosf(f);
#else
    return cosf(f);
#endif
}

float OmlMath_Atan2f(float f1, float f2) {
#ifdef OML_WASM
    return _oml_atan2f(f1, f2);
#else
    return atan2f(f1, f2);
#endif
}

float OmlMath_Sqrtf(float f) {
#ifdef OML_WASM
    return _oml_sqrtf(f);
#else
    return sqrtf(f);
#endif
}
