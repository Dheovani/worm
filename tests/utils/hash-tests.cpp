#include <utils/hash.hpp>

int main()
{
    static_assert(worm::HashCode("") == 0);
    static_assert(worm::HashCode("a") == 97);
    static_assert(worm::HashCode("ab") == 3687);
    static_assert(worm::HashCode("entity") == worm::HashCode("entity"));
    static_assert(worm::HashCode("entity") != worm::HashCode("Entity"));

    return 0;
}
