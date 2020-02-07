from cv2 import cv2
import os
import numpy as np
import imutils
from recorder_console import parse_args, \
    mkdir_if_not_exists, read_sensor_images, \
    read_sensor_poses, synchronize_sensor_frames

workspace_path = "C:/recordings"

recording = "HoloLensRecording__2020_01_27__08_42_51"

recording_path = os.path.join(workspace_path, recording)
reconstruction_path = os.path.join(recording_path, "reconstruction")
image_path = os.path.join(reconstruction_path, "images")

camera_width = 640
camera_height = 480
camera_params = { # fx, fy, cx, cy, k1, k2, p1, p2
    "vlc_ll": "450.072070 450.274345 320 240 "
                "-0.013211 0.012778 -0.002714 -0.003603",
    "vlc_lf": "448.189452 452.478090 320 240 "
                "-0.009463 0.003013 -0.006169 -0.008975",
    "vlc_rf": "449.435779 453.332057 320 240 "
                "-0.000305 -0.013207 0.003258 0.001051",
    "vlc_rr": "450.301002 450.244147 320 240 "
                "-0.010926 0.008377 -0.003105 -0.004976",
    "pv": "3124.6464 3122.8344 86.555904 64.628136 "
                "0 0 0 0", # TODO Check values
}

scaler = lambda s: np.array([[s, 0, 0, 0],
                             [0, s, 0, 0],
                             [0, 0, s, 0],
                             [0, 0, 0, 1]])

translater = lambda x,y,z: np.array([[x],
                                     [y],
                                     [z],
                                     [0]])

#################################################################################

def getCube(pos, scale):
    cube = [ np.array([[i],[j],[k],[1]])
            for i,j,k in [(.5, .5, -.5), (-.5, .5, .5), (.5, -.5, .5), (.5, .5, .5),
                        (-.5, -.5, .5), (.5, -.5, -.5), (-.5, .5, -.5), (-.5, -.5, -.5)] ]

    cube = list(map(lambda p: np.dot(scaler(scale), p), cube))
    cube = list(map(lambda p: np.add(p, translater(pos[0], pos[1], pos[2])), cube))
    return cube

def getCameraParams(camera_name):
    ''' Returns the intrinsic camera parameters matrix '''
    camera_params_list = list(map(float, camera_params[camera_name].split()))
    fx, fy, cx, cy, _, _, _, _ = np.array(camera_params_list, dtype=np.double)
    K = np.array([[fx,  0, cx, 0],
                  [ 0, fy, cy, 0],
                  [ 0,  0,  1, 0],
                  [ 0,  0,  0, 1]])
    return K

def renderPose(img, pose, camera_name, height, width, cube):
    K = getCameraParams(camera_name)
    P = K @ pose # @ is matrix multiplication (numpy)
    for p in cube:
        uv = P @ p
        if uv[2] == 0: continue
        u, v = uv[0]/uv[2], uv[1]/uv[2]
        if 0 <= u and u <= height and 0 <= v and v <= width:
            cv2.circle(img, (u, v), 10, (0,255,0), -1)
    return img
      
#################################################################################

if __name__ == "__main__":
    args = parse_args()
    if "vlc" in args.ref_camera_name:
        camera_names = ["vlc_ll", "vlc_lf", "vlc_rf", "vlc_rr"]
        camera_width = 640
        camera_height = 480
    elif "pv" in args.ref_camera_name:
        camera_names = ["pv"]
        camera_width = 1280
        camera_height = 720
    elif "long_throw" in args.ref_camera_name:
        camera_names = ["long_throw_depth", "long_throw_reflectivity"]
        camera_width = 640
        camera_height = 480
    elif "short_throw" in args.ref_camera_name:
        camera_names = ["short_throw_depth", "short_throw_reflectivity"]
        camera_width = 640
        camera_height = 480

    frames, poses = synchronize_sensor_frames(
        args, recording_path, image_path, camera_names)

    # Draw Sample
    cube = getCube([0.2, 0, -.75], 0.1)

    # Looping through frames
    for id in range(len(frames)):
        frame = frames[id]
        pose = poses[id]

        if len(frame) == 0:
            continue

        final = np.zeros((camera_width, 0, 3), np.uint8)
        # Looping through sensors
        for l in range(len(camera_names)):
            try:
                img = cv2.imread(os.path.join(image_path, frame[l]))
            except:
                img = np.zeros((camera_height, camera_width, 3), np.uint8)
            img = renderPose(img, pose[l], camera_names[l], camera_height, camera_width, cube)

            if "vlc" in camera_names[l]:
                imgout = imutils.rotate_bound(img, 90)
                final = cv2.hconcat((final, imgout))
            else:
                final = img
        cv2.imshow('Pose Rendering', final)
        cv2.waitKey((int) (1000/args.frame_rate))
    cv2.destroyAllWindows()