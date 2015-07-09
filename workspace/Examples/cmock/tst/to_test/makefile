TEST_PROG = to_test_test
TEST_OBJS = ../../src/to_test.o
TEST_SRC = dependency_mock.cpp main.cpp
TEST_SRC_OBJS = $(TEST_SRC:.cpp=.o)
CXXFLAGS = -Wall -g -I../../src

STUBS_SRC = ../../src/dependency.c
STUB_LIB = libstubs.so
STUB_LDFLAG = -L. -lstubs

all: $(TEST_PROG)

$(TEST_PROG): $(STUB_LIB) $(TEST_SRC_OBJS)
	$(CXX) $(TEST_OBJS) $(TEST_SRC_OBJS) -o $(TEST_PROG) -rdynamic -Wl,--no-as-needed -ldl -pthread $(STUB_LDFLAG) -lgmock_main -lgmock -lgtest

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(STUB_LIB):
	$(CC) -fPIC $(STUBS_SRC) -shared -Wl,-soname,./$@ -o $@

clean:
	rm -f $(TEST_PROG)
	rm -f $(TEST_SRC_OBJS)
	rm -f $(STUB_LIB)

