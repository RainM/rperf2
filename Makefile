LIBIPT_FOLDER=$(PWD)/processor-trace/
INSTRUMENTER_FOLDER=$(PWD)/instrumenter/
GSON_TESTER_FOLDER=$(PWD)/gson-tester/

OPTFLAGS?=-O3 -flto
#OPTFLAGS+=-fsanitize=address -fno-omit-frame-pointer
CFLAGS=-g -pedantic -fpic -march=native -DENABLE_DEBUG1
CFLAGS+=$(OPTFLAGS)


build-libipt:
	rm -rf $(LIBIPT_FOLDER)/build
	rm -rf $(LIBIPT_FOLDER)/install
	mkdir -p $(LIBIPT_FOLDER)/build
	mkdir -p $(LIBIPT_FOLDER)/install
	cd $(LIBIPT_FOLDER)/build ; cmake -DCMAKE_INSTALL_PREFIX=$(LIBIPT_FOLDER)/install -DCMAKE_BUILD_TYPE=Release ..
	make clean  install -C $(LIBIPT_FOLDER)/build -j8

build-instrumenter:
	cd $(INSTRUMENTER_FOLDER) ; mvn $(MVNFLAGS) clean package

build-gson-tester:
	cd $(GSON_TESTER_FOLDER) ; mvn $(MVNFLAGS) clean package

build-cpp-tester:
	make -C cpp-tester build-cpp-tester

build-librperf2-java: build-librperf2
	cd librperf2-java && javah -cp ../instrumenter/target/classes/ ru.raiffeisen.PerfPtProf
	CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS) -I../librperf2" LDFLAGS="-L../librperf2" make build-librperf2-java -C librperf2-java

build-librperf2-cpp: build-librperf2
	CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS) -I../librperf2" LDFLAGS="-L../librperf2" make build-librperf2-cpp -C librperf2-cpp

build-librperf2:
	CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" make build-librperf2 -C librperf2

run-gson-tester-999:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2-java:processor-trace/install/lib64 \
		java \
			-Xmx30g -Xmn25g \
			-XX:MaxInlineSize=13500 \
			-DTRIGGER_METHOD=decode	 		\
			-DTRIGGER_CLASS=ru/raiffeisen/App2 	\
			-DTRIGGER_COUNTDOWN=15500 		\
			-DPERCENTILE=0.999 			\
			-DTRACE_DEST=trace_999.out 		\
			-DTRACE_MAX_SZ=100000000 		\
			-agentlib:rperf2-java 			\
			-javaagent:instrumenter/target/instrumenter-1.2-SNAPSHOT-jar-with-dependencies.jar \
			-cp gson-tester/target/json-tester-1.0-SNAPSHOT.jar:gson-tester/target/lib/gson-2.8.2.jar  \
			ru.raiffeisen.App2

run-gson-tester-median:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2-java:processor-trace/install/lib64 \
		java \
			-Xmx30g -Xmn25g \
			-XX:MaxInlineSize=13500 \
			-DTRIGGER_METHOD=decode	 		\
			-DTRIGGER_CLASS=ru/raiffeisen/App2 	\
			-DTRIGGER_COUNTDOWN=20000 		\
			-DTRACE_DEST=trace_median.out 		\
			-DTRACE_MAX_SZ=100000000 		\
			-agentlib:rperf2-java 			\
			-javaagent:instrumenter/target/instrumenter-1.2-SNAPSHOT-jar-with-dependencies.jar \
			-cp gson-tester/target/json-tester-1.0-SNAPSHOT.jar:gson-tester/target/lib/gson-2.8.2.jar  \
			ru.raiffeisen.App2

run-gson-tester:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2-java:processor-trace/install/lib64 \
		java \
			-Xmx30g -Xmn25g \
			-XX:MaxInlineSize=13500 \
			-XX:+PreserveFramePointer \
			-cp gson-tester/target/json-tester-1.0-SNAPSHOT.jar:gson-tester/target/lib/gson-2.8.2.jar  \
			ru.raiffeisen.App2

run-cpp-tester-O0-999:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2-cpp:processor-trace/install/lib64 		\
		PT_PROF_SKIP_N=10000 								\
		PT_PROF_PERCENTILE=0.9999 							\
		PT_PROF_TRACE_DEST=cpp_trace 							\
	 ./cpp-tester/tester_o0

run-cpp-tester-O3-999:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2-cpp:processor-trace/install/lib64 		\
		PT_PROF_SKIP_N=300000 								\
		PT_PROF_PERCENTILE=0.9999 							\
		PT_PROF_TRACE_DEST=cpp_trace 							\
	 ./cpp-tester/tester_o3

clean:
	find . -name '*.o' | xargs rm
	find . -name '*.a' | xargs rm
	find . -name '*.so' | xargs rm
