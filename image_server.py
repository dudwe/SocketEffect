
import socket
import base64
import numpy as np
import cv2



HEADERSIZE = 10

s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
s.bind(('127.0.0.1',8080))
s.listen(5)



while True:
    #Move conn up for continous stream
    clientsocket,address = s.accept()
    print(f"Connection from {address} has been established")

    #Recv data from server
    full_msg = ''
    new_msg = True

    while True:
        msg = clientsocket.recv(4096)
        if(new_msg):
            print(msg)
            print(f"Client message length: {msg[:HEADERSIZE]}")
            msglen = int(msg[:HEADERSIZE])
            print(msglen)
            new_msg = False
        full_msg += msg.decode("utf-8")
        print(len(full_msg),msglen)
        if(len(full_msg)-HEADERSIZE >= msglen):
            print("Received data from client")
            ##print(full_msg[HEADERSIZE:])
            break
    #Decode string of base64 
    img_data = full_msg[HEADERSIZE:]

    img_data = base64.b64decode(img_data)
    #Get byte object from string


    #Convert byes

    img_data = np.fromstring(img_data,dtype=np.uint8)
   
    

    print(img_data.shape)
    #Reshape
    img_data = np.reshape(img_data,(128,128,3))
    print(img_data.shape)
    cv2.imshow('image from client',img_data)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    
    #print(img_data)
    #Send grayscale image to client

    grayImage = cv2.cvtColor(img_data, cv2.COLOR_BGR2GRAY)
    print(grayImage.shape)

            
    #Convert image to string
    img_str = base64.b64encode(grayImage)
    #print(len(img_str))
#    print(img_str)
    print(np.fromstring(base64.b64decode(img_str),dtype=np.uint8))
    #Send image as string to client
    img_str = f'{len(img_str):<{HEADERSIZE}}'+img_str.decode("utf-8")
    #s.send(img_str)

    #print(bytes(img_str,"utf-8"))

    clientsocket.send(bytes(img_str,"utf-8"))



    break

