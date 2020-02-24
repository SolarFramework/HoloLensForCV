import grpc
import logging
import numpy as np
from cv2 import cv2
import base64
import os, sys
import time

import SensorStreaming_pb2
import SensorStreaming_pb2_grpc

def makeNameRPC(camera_name):
    return SensorStreaming_pb2.NameRPC(cameraName=camera_name)

def connect():
    channel = grpc.insecure_channel("0.0.0.0:50051")
    stub = SensorStreaming_pb2_grpc.StreamerStub(channel)
    print("Connected to the device!")
    return stub

def enable(sensorList):
    sensorListRPC = SensorStreaming_pb2.SensorListRPC()
    for sensor in sensorList:
        newSensor = sensorListRPC.sensor.add()
        newSensor.cameraName = sensor
    response = stub.EnableSensors(sensorListRPC)
    print("Enabled sensors ", [name.cameraName for name in response.sensor] )

def getIntrinsics(stub, camera_name):
    print(f"GetIntrinsics {camera_name} request")
    camIntrinsics = stub.GetCamIntrinsics(makeNameRPC(camera_name))
    print(camIntrinsics)

def streamSensors(stub, camera_name):
    print(f"StreamSensors {camera_name} request")
    reader = stub.SensorStream(makeNameRPC(camera_name))
    print("Reader...")
    return reader

def process(reader):
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
    # enable(["vlc_lf", "vlc_rf"])
    getIntrinsics(stub, "vlc_lf")
    reader = streamSensors(stub, "vlc_lf")
    process(reader)

    # with open("log.txt", 'r') as f:
    #     data = f.read()
    #     print(data)
    #     nparr = np.frombuffer(data, np.byte)
    #     print(nparr)
    #     img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    #     print(img)