import grpc
import logging
import imutils
import numpy as np
from cv2 import cv2
import base64
import os, sys
import time

import SensorStreaming_pb2
import SensorStreaming_pb2_grpc

from reader import renderPose, getCube

def getPose(cameraView, frameToOrigin):
    camera_to_image = np.array(# flip axis for OpenCV camera model
                           [[1, 0, 0, 0],
                            [0, -1, 0, 0],
                            [0, 0, -1, 0],
                            [0, 0, 0, 1]])
    
    if abs(np.linalg.det(frameToOrigin) - 1) < 0.01:
        return camera_to_image @ cameraView @ np.linalg.inv(frameToOrigin)
    return np.zeros((4,4))

def parseMatRPC(m):
    return np.array([[m.m11, m.m21, m.m31, m.m41],
                     [m.m12, m.m22, m.m32, m.m42],
                     [m.m13, m.m23, m.m33, m.m43],
                     [m.m14, m.m24, m.m34, m.m44]])

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

def process(reader, cube):
    fps = 25
    for sensorFrame in reader:
        # print("frame:")
        # print(sensorFrame.image.width)
        # print(sensorFrame.image.height)
        # print(sensorFrame.pose)
        width = sensorFrame.image.width
        height = sensorFrame.image.height
        cameraView = parseMatRPC(sensorFrame.pose.cameraView)
        frameToOrigin = parseMatRPC(sensorFrame.pose.frameToOrigin)
        pose = getPose(cameraView, frameToOrigin)
        print("cameraView\n", cameraView)
        print("frameToOrigin\n", frameToOrigin)
        print("pose\n", pose)
        img = createImg(width, height, sensorFrame.image.data)
        img = renderPose(img, pose, "vlc_lf", height, width, cube)
        img = imutils.rotate_bound(img, 90)
        cv2.imshow("Pose Rendering", img)
        cv2.waitKey(0)
    cv2.destroyAllWindows()

def readb64(base64_string):
    nparr = np.frombuffer(base64_string, np.uint8)
    nparr = np.reshape(nparr, (480, 640, 1))
    return cv2.cvtColor(nparr, cv2.COLOR_GRAY2BGR)

def createImg(width, height, data):
    # nparr = np.frombuffer(bytes(data), np.uint8)
    # return np.reshape(nparr, (height, width, 3))
    return np.zeros((width, height, 3), np.uint8)    

if __name__ == '__main__':
    logging.basicConfig()

    cube = getCube([0.2, 0, -.75], 0.1)

    stub = connect()
    # enable(["vlc_lf", "vlc_rf"])
    getIntrinsics(stub, "vlc_lf")
    reader = streamSensors(stub, "vlc_lf")
    process(reader, cube)

    # with open("log.txt", 'r') as f:
    #     data = f.read()
    #     print(data)
    #     nparr = np.frombuffer(data, np.byte)
    #     print(nparr)
    #     img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    #     print(img)