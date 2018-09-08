LIBIPT_FOLDER=$(PWD)/processor-trace/
INSTRUMENTER_FOLDER=$(PWD)/instrumenter/
GSON_TESTER_FOLDER=$(PWD)/gson-tester/

CXX=/opt/rh/devtoolset-7/root/usr/bin/g++
CC=/opt/rh/devtoolset-7/root/usr/bin/gcc
OPTFLAGS?=-O3 -flto
#OPTFLAGS?=-O1
# -fsanitize=address -fno-omit-frame-pointer
CFLAGS=-g -pedantic -fpic
CFLAGS+=$(OPTFLAGS)


build-libipt:
	rm -rf $(LIBIPT_FOLDER)/build
	rm -rf $(LIBIPT_FOLDER)/install
	mkdir -p $(LIBIPT_FOLDER)/build
	mkdir -p $(LIBIPT_FOLDER)/install
	cd $(LIBIPT_FOLDER)/build ; cmake -DCMAKE_INSTALL_PREFIX=$(LIBIPT_FOLDER)/install -DCMAKE_BUILD_TYPE=Release ..
	make clean  install -C $(LIBIPT_FOLDER)/build -j8

build-instrumenter:
	cd $(INSTRUMENTER_FOLDER) ; mvn -Djavax.net.ssl.trustStore=/home/ruamnsa/dev/ET-XXXX-docker/scripts/cks.jks clean package

build-gson-tester:
	cd $(GSON_TESTER_FOLDER) ; mvn -Djavax.net.ssl.trustStore=/home/ruamnsa/dev/ET-XXXX-docker/scripts/cks.jks clean package

build-librperf2:
	cd librperf2 ; javah -cp ../instrumenter/target/classes/ ru.raiffeisen.PerfPtProf
	CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" make librperf2 -C librperf2

run-gson-tester-999:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2:processor-trace/install/lib64 \
		java \
			-XX:+UseConcMarkSweepGC \
			-DTRIGGER_METHOD=decode \
			-DTRIGGER_CLASS=ru/raiffeisen/App \
			-DTRIGGER_COUNTDOWN=5000 \
			-DPERCENTILE=0.999 \
			-DTRACE_DEST=trace.out \
			-DTRACE_MAX_SZ=100000000 \
			-agentlib:rperf2 \
			-javaagent:instrumenter/target/instrumenter-1.2-SNAPSHOT-jar-with-dependencies.jar \
			-cp gson-tester/target/json-tester-1.0-SNAPSHOT.jar:gson-tester/target/lib/gson-2.8.2.jar  \
			ru.raiffeisen.App

run-gson-tester-median:
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):librperf2:processor-trace/install/lib64 \
		java \
			-DTRIGGER_METHOD=decode \
			-DTRIGGER_CLASS=ru/raiffeisen/App \
			-DTRIGGER_COUNTDOWN=2000 \
			-DOUTPUT_INSTRUMENTED_CLASSES=1 \
			-DTRACE_DEST=trace.out \
			-agentlib:rperf2 \
			-javaagent:instrumenter/target/instrumenter-1.2-SNAPSHOT-jar-with-dependencies.jar \
			-cp gson-tester/target/json-tester-1.0-SNAPSHOT.jar:gson-tester/target/lib/gson-2.8.2.jar  \
			ru.raiffeisen.App
