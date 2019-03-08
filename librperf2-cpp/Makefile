CXXFLAGS=-std=c++11

build-librperf2-cpp:
	$(CXX) cpp_impl.cpp -c $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) cpp_impl.o -lrperf2 -shared -o librperf2-cpp.so -lipt -L../processor-trace/install/lib64 -rdynamic -pie -pthread -ldl
