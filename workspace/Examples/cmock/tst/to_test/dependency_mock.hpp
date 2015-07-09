#include <cmock/cmock.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dependency.h"

#ifdef __cplusplus
}
#endif


DECLARE_FUNCTION_MOCK1(BarFunctionMock, bar, int(int));

