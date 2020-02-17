all:
	python -m grpc_tools.protoc -I../../Tools/Tasks/protos --python_out=. --grpc_python_out=. ../../Tools/Tasks/protos/SensorStreaming.proto
	@echo "Done!"

clean:
	rm SensorStreaming_pb2_grpc.py
	rm SensorStreaming_pb2.py