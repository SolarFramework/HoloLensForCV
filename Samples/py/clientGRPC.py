import grpc
import logging

import SensorStreaming_pb2
import SensorStreaming_pb2_grpc

def makeNameRPC(camera_name):
    return SensorStreaming_pb2.NameRPC(cameraName=camera_name)

def connect():
    channel = grpc.insecure_channel("10.10.255.241:50051")
    stub = SensorStreaming_pb2_grpc.StreamerStub(channel)
    return stub

def getIntrinsics(stub):
    camIntrinsics = stub.GetCamIntrinsics(makeNameRPC("vlc_lf"))
    print(camIntrinsics)

def streamSensors(stub, camera_name):
    reader = stub.SensorStream(makeNameRPC(camera_name))

    for sensorFrame in reader:
        print(sensorFrame)

if __name__ == '__main__':
    logging.basicConfig()
    stub = connect()
    getIntrinsics(stub)
    # streamSensors(stub, "vlc_lf")