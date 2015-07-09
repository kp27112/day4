#include <stdio.h>
#include "to_test.h"

int main(int argc, const char *argv[])
{
    int res = foo(3, 4);
    printf("3 + (2*4) = %d\n", res);
	return 0;
}

