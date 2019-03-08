CXXFLAGS=-std=c++11

build-librperf2:
	$(CXX) prof_impl.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	$(CXX) rcollect.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	$(CXX) pt_parser.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	$(CXX) rdtsc_utils.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	-rm librperf2.a
	ar rcs librperf2.a rcollect.o pt_parser.o prof_impl.o rdtsc_utils.o

