#ifdef OML_WASM
#define OML_IMPORT(n) __attribute__((import_module("env"), import_name(n)))
__attribute__((import_module("env"), import_name("sinf"))) float
_oml_sinf(float);
__attribute__((import_module("env"), import_name("cosf"))) float
_oml_cosf(float);
__attribute__((import_module("env"), import_name("atan2f"))) float
_oml_atan2f(float, float);
#else
#include <math.h>
#endif

static unsigned long GLOBAL_SEED = 1UL;

unsigned long bitshift_rand(void) {
    unsigned long x = GLOBAL_SEED;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    // Mask to 32 bit
    x &= 0xFFFFFFFFUL;

    GLOBAL_SEED = x;
    return x;
}

// Function to set the global seed
void OmlMath_SetRandomizerSeed(unsigned long seed) {
    if (seed == 0) {
        seed = 314159265UL;
    }
    GLOBAL_SEED = seed;
}

float OmlMath_RandRangeFloat(float min, float max) {
    if (min >= max) {
        return min;
    }
    // get a float from 0 to 1
    float scale = (float)bitshift_rand() / 4294967295.0f;

    // lerp
    return min + scale * (max - min);
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
