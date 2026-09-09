#include <stdio.h>
#include "helloworld_version.h"

int
main(void)
{
  printf("Hello world!\n");
  printf("---v%d.%d.%d---\n", HELLOWORLD_VERSION_MAJOR,
         HELLOWORLD_VERSION_MINOR, HELLOWORLD_VERSION_PATCH);
  return 0;
}
