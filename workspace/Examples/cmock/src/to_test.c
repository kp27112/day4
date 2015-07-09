#include "to_test.h"
#include "dependency.h"

int foo(int a, int b)
{
    return a + bar(b);
}

