
all:
	protoc -I ./protos --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`.exe ./protos/sensorStreaming.proto

	protoc -I ./protos --cpp_out=. ./protos/sensorStreaming.proto

clean:
	rm -f *.o *.pb.cc *.pb.h