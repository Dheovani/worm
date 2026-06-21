#include <utils/hash.hpp>

int main()
{
  static_assert(worm::hashCode("") == 0);
  static_assert(worm::hashCode("a") == 97);
  static_assert(worm::hashCode("ab") == 3687);
  static_assert(worm::hashCode("entity") == worm::hashCode("entity"));
  static_assert(worm::hashCode("entity") != worm::hashCode("Entity"));

  return 0;
}
