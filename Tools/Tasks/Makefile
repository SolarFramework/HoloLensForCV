# TODO: edit generated files to add '#include "pch.h"' at the beginning
# For now do it manually
all:
	mkdir build
	protoc -I ./protos --grpc_out=./build/ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`.exe ./protos/sensorStreaming.proto

	protoc -I ./protos --cpp_out=./build/ ./protos/sensorStreaming.proto
	@echo "Done!"
	@echo
	@echo "/!\\ Do not forget to add the line '#include \"pch.h\"' at the beginning of both .cc files! /!\\"

clean:
	rm -rf build/