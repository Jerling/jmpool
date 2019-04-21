#include "jmpool.h"

int main(int argc, char *argv[]) {
  auto alloc = jmp::jmp_allocator(
      jlog::logger::new_logger("jmp", {1}, jlog::log_level_t::info));
  char *test_b = (char *)alloc.allocate(4096);
  strcpy(test_b, "small test");
  INFO("test_b is %s on %p and size is %d\n", test_b, test_b, 4096);
  alloc.deallocate(test_b, 4096);

  char *test_s = (char *)alloc.allocate(32);
  strcpy(test_s, "big test");
  INFO("test_s is %s on %p and size is %d\n", test_s, test_s, 32);
  alloc.deallocate(test_s, 32);

  int *p_int = nullptr;
  p_int = (int *)alloc.allocate(sizeof(int));
  jmp::details::_construct(p_int, 1);
  INFO("Construct in %p with value is %d and size is %d\n", p_int, *p_int,
       sizeof(int));
  alloc.deallocate(p_int, sizeof(int));

  return 0;
}
