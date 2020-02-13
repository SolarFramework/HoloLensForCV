
all:
	mkdir build
	protoc -I ./protos --grpc_out=./build/ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`.exe ./protos/sensorStreaming.proto

	protoc -I ./protos --cpp_out=./build/ ./protos/sensorStreaming.proto

clean:
	rm -rf build/