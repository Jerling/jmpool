#include "jmpool.h"

int main(int argc, char *argv[]) {
  int *p = (int *)malloc(sizeof(int));
  jmp::details::_construct(p, 12);
  printf("%d\n", *p);
  jmp::details::_destory(p);
  return 0;
}
