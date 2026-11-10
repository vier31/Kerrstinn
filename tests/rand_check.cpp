// kerr::Rand's Mac flavour against this machine's rand(), for every seed the
// Random knob can produce.
#include "KerrRand.h"

#include <cstdio>
#include <cstdlib>

int main()
{
    kerr::Rand r;
    for (unsigned seed = 0; seed <= 65535; seed++)
    {
        std::srand (seed);
        r.seed (seed);
        for (int i = 0; i < 64; i++)
            if (std::rand() != r.next())
            {
                std::printf ("rand_check: seed %u differs at call %d\n", seed, i);
                return 1;
            }
    }
    std::printf ("rand_check: Mac flavour matches rand() for seeds 0..65535\n");
    return 0;
}
