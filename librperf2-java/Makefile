JAVA_BIN_PATH=$(shell dirname $(shell readlink -f $(shell which javac)))
JNI_HEADERS_PATH=$(JAVA_BIN_PATH)/../../include/ $(JAVA_BIN_PATH)/../include/
JNI_INCLUDE_PATHS=$(JNI_HEADERS_PATH) $(addsuffix linux/,$(JNI_HEADERS_PATH))
JNI_INCLUDE_FLAGS=$(addprefix -I,$(JNI_INCLUDE_PATHS))

CXXFLAGS=-std=c++11

build-librperf2-java:
	$(CXX) jvmti_agent.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS)
	$(CXX) jni_impl.cpp -c $(JNI_INCLUDE_FLAGS) $(CXXFLAGS) $(CFLAGS) -I../processor-trace/install/include/
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) jni_impl.o jvmti_agent.o -lrperf2 -shared -o librperf2-java.so -lipt -L../processor-trace/install/lib64 -rdynamic -pie -pthread -ldl
