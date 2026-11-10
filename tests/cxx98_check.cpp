// Compiled as C++98 with warnings as errors (see CMakeLists.txt).
#include "KerrStinn.h"

struct Int16Buffer
{
    short* p;
    float get (int i) const { return p[i] / 32768.0f; }
    void set (int i, float v) { p[i] = (short) (v * 32767.0f); }
};

template class kerr::KerrStinn<Int16Buffer>;
template class kerr::Engine<Int16Buffer>;
