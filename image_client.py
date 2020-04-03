import socket
import cv2
import numpy as np
import base64
HEADERSIZE = 10

s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

s.connect((socket.gethostname(),1234))

while True:
    #Send image to server
    img = cv2.imread('greggs.jpg')
    #Resize the image
    img = cv2.resize(img,(128,128))
    #Convert image to string
    img_str = base64.b64encode(img)
    #Send image as string to client
    img_str = f'{len(img_str):<{HEADERSIZE}}'+img_str.decode("utf-8")
    #s.send(img_str)
    s.send(bytes(img_str,"utf-8"))

    full_msg = ''
    new_msg = True

    #recv data from server
    while True:
        msg = s.recv(4096)
        if(new_msg):
            print(f"new message length: {msg[:HEADERSIZE]}")
            msglen = int(msg[:HEADERSIZE])
            new_msg = False

        full_msg += msg.decode("utf-8")

        if(len(full_msg)-HEADERSIZE == msglen):
            print("full msg recvd")
            break
    #Decode string of base64
    img_data = full_msg[HEADERSIZE:]
    img_data = base64.b64decode(img_data)
    #Get byte object from string
    #Convert byes
    img_data = np.fromstring(img_data,dtype=np.uint8)
    print(img_data.shape)
    #Reshape
    img_data = np.reshape(img_data,(128,128))
    print(img_data.shape)

    cv2.imshow('image from server',img_data)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    
    
    break

