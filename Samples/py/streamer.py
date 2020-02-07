import websockets
import asyncio
import imutils
import base64
import numpy as np
import json
from cv2 import cv2
import time

from reader import scaler, translater, getCube, renderPose

camera_to_image = np.array(# flip axis for OpenCV camera model
                           [[1, 0, 0, 0],
                            [0, -1, 0, 0],
                            [0, 0, -1, 0],
                            [0, 0, 0, 1]])

def getPose(frame_to_origin, viewMatrix, projMatrix):
    frame_to_origin = np.array(list(map(float, frame_to_origin.split(',')))).reshape(4,4).T
    viewMatrix = np.array(list(map(float, viewMatrix.split(',')))).reshape(4,4).T
    # projMatrix = np.array(list(map(float, projMatrix.split(',')))).reshape(4,4).T

    if abs(np.linalg.det(frame_to_origin) - 1) < 0.01:
        return camera_to_image @ viewMatrix @ np.linalg.inv(frame_to_origin)
    return np.zeros((4,4))

async def streamData(websocket, path):
    print("Connected to Hololens. Begin streaming data...")
    running = True
    fps = 0
    begin, end = 0, 0
    cube = getCube([0, 0, -0.75], 0.1)
    while running:
        begin = time.time()
        try:    
            msg = await websocket.recv()
        except:
            print("Connection Lost, resetting...")
            running = False
            cv2.destroyAllWindows()
            break

        data = json.loads(msg)
        if data['topic'] != '/hololens':
            continue
        stream = data['msg']
        # timestamp = stream['timestamp']
        # print(time.ctime(timestamp/1e7)[:-5])
        height = stream['height']
        width = stream['width']
        leftData = stream['left64']
        rightData = stream['right64']
        leftF2O = stream['leftFrameToOrigin']
        leftViewMatrix = stream['leftCameraViewTransform']
        leftProjMatrix = stream['leftCameraProjectionTransform']
        rightF2O = stream['rightFrameToOrigin']
        rightViewMatrix = stream['rightCameraViewTransform']
        rightProjMatrix = stream['rightCameraProjectionTransform']

        # Left
        leftImg = readb64(leftData)
        leftPose = getPose(leftF2O, leftViewMatrix, leftProjMatrix)
        leftImg = renderPose(leftImg, leftPose, "vlc_lf", height, width, cube)
        leftImg = imutils.rotate_bound(leftImg, 90)
        # Right
        rightImg = readb64(rightData)
        rightPose = getPose(rightF2O, rightViewMatrix, rightProjMatrix)
        rightImg = renderPose(rightImg, rightPose, "vlc_rf", height, width, cube)
        rightImg = imutils.rotate_bound(rightImg, 90)

        stereo = cv2.hconcat((leftImg, rightImg))
        cv2.putText(stereo, f"FPS: {fps}", (30, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,0))
        cv2.imshow("Stream", stereo)
        cv2.waitKey(1)

        end = time.time()
        fps = (int) (1 / (end - begin))

async def hello(websocket, path):
    count = 0
    while count < 3:
        msg = await websocket.recv()
        data = json.loads(msg)
        if data['op'] == 'advertise':
            print(f"< {data['op']}{data['topic']} -> {data['type']}")
        else:
            print(f"< {data['op']}{data['topic']} -> {data['msg']['data']}")
        count += 1
    await streamData(websocket, path)

def readb64(base64_string):
    nparr = np.frombuffer(base64.b64decode(base64_string), np.uint8)
    nparr = np.reshape(nparr, (480, 640, 1))
    return cv2.cvtColor(nparr, cv2.COLOR_GRAY2BGR)

if __name__ == "__main__":
    start_server = websockets.serve(hello, "10.10.254.226", 9090)
    print("Serving socket on 10.10.254.226:9090")
    print("Waiting for connection from the HoloLens...")
    asyncio.get_event_loop().run_until_complete(start_server)
    asyncio.get_event_loop().run_forever()