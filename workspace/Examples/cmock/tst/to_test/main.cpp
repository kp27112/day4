#ifdef __cplusplus
extern "C" {
#endif

#include "to_test.h"

#ifdef __cplusplus
}
#endif

#include "dependency_mock.hpp"

using namespace ::testing;

TEST(foo_test_suite, TC_01)
{
    {
        BarFunctionMock bfm;
        EXPECT_FUNCTION_CALL(bfm, (4)).WillOnce(Return(4));
        ASSERT_EQ(7, foo(3, 4));
    }
    ASSERT_EQ(11, foo(3, 4));
}

/*int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}*/

