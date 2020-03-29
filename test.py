import socket
import cv2
import numpy as np
import base64
HEADERSIZE = 1024





#Send image to server
img = cv2.imread('greggs.jpg')
#Resize the image
img = cv2.resize(img,(128,128))


print(img)
#Convert image to string
img_str = base64.b64encode(img)

print(len(img_str))

tmp = base64.b64decode(img_str)
tmp = np.fromstring(tmp,dtype=np.uint8)
print(tmp)
print(tmp.shape)
#Send image as string to client

#img_str = f'{len(img_str):<{HEADERSIZE}}'+base64.b64decode(img_str).decode('utf-8')
    

