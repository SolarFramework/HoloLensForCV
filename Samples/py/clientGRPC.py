import grpc
import logging
import numpy as np
from cv2 import cv2
import base64

import SensorStreaming_pb2
import SensorStreaming_pb2_grpc

def makeNameRPC(camera_name):
    return SensorStreaming_pb2.NameRPC(cameraName=camera_name)

def connect():
    channel = grpc.insecure_channel("0.0.0.0:50051")
    stub = SensorStreaming_pb2_grpc.StreamerStub(channel)
    return stub

def getIntrinsics(stub, camera_name):
    camIntrinsics = stub.GetCamIntrinsics(makeNameRPC(camera_name))
    print(camIntrinsics)

def streamSensors(stub, camera_name):
    reader = stub.SensorStream(makeNameRPC(camera_name))
    print("Reader...")
    for sensorFrame in reader:
        print("frame:")
        print(sensorFrame.image.width)
        print(sensorFrame.image.height)
        print(sensorFrame.pose)

def readb64(base64_string):
    nparr = np.frombuffer(base64_string, np.uint8)
    nparr = np.reshape(nparr, (480, 640, 1))
    return cv2.cvtColor(nparr, cv2.COLOR_GRAY2BGR)

if __name__ == '__main__':
    logging.basicConfig()
    stub = connect()
    getIntrinsics(stub, "vlc_lf")
    streamSensors(stub, "vlc_lf")

    # with open("log.txt", 'r') as f:
    #     data = f.read()
    #     print(data)
    #     nparr = np.frombuffer(data, np.byte)
    #     print(nparr)
    #     img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    #     print(img)