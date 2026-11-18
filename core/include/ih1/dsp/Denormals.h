// RAII flush-to-zero / denormals-are-zero guard (KADAL KNW-PY324W089P).
#pragma once

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <xmmintrin.h>
#define IH1_HAS_SSE 1
#endif

#include <cstdint>

namespace ih1::dsp {

class ScopedNoDenormals {
public:
    ScopedNoDenormals() {
#if IH1_HAS_SSE
        prev_ = _mm_getcsr();
        _mm_setcsr(prev_ | 0x8040u);  // FTZ | DAZ
#elif defined(__aarch64__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        prev_ = fpcr;
        fpcr |= (1ull << 24);  // FZ
        asm volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
    }
    ~ScopedNoDenormals() {
#if IH1_HAS_SSE
        _mm_setcsr(static_cast<unsigned>(prev_));
#elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(prev_));
#endif
    }
    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
    uint64_t prev_ = 0;
};

// Flush a tiny value to exactly zero (use on feedback states at block boundaries).
inline float flushTiny(float x) { return (x > -1e-20f && x < 1e-20f) ? 0.0f : x; }

}  // namespace ih1::dsp
