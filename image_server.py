import time
import sys
import socket
import base64
import numpy as np
import cv2
import tensorflow as tf
import IPython.display as display
from PIL import Image
import numpy as np
import matplotlib.pyplot as plt
import os
#https://www.tensorflow.org/tutorials/load_data/images
import time
from tensorflow.python.client import timeline
import cv2
from tensorflow.python.client import device_lib
print(device_lib.list_local_devices())

HEADERSIZE = 10


def model_load(model='unet-coco.h5'):
    print("Loading Model...")
    #Init Tensorflow
    loaded_model = tf.keras.models.load_model(model)
    return loaded_model

def start_server(ip='127.0.0.1',port=8080):
    print("Starting Server...")
    s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.bind((ip,port))
    s.listen(5)
    return s
    
def main(loaded_model,s,ip='127.0.0.1',port=8080):




    

    #Move conn up for continous strea
    
    #clientsocket,address = s.accept()
    
    #print(f"Connection from {address} has been established") 

    clientsocket,address = s.accept()
    print(f"Connection from {address} has been established")    
    while True:

        #Recv data from server
        full_msg = ''
        new_msg = True
    
        while True:
            msg = clientsocket.recv(4096)
            if(new_msg):
                #If nothing, the session has been closed and we make a new one
                if(len(msg)==0):
                    return
                print(f"Client message length: {msg[:HEADERSIZE]}")
                msglen = int(msg[:HEADERSIZE])
                new_msg = False
                
            full_msg += msg.decode("utf-8")
            if(len(full_msg)-HEADERSIZE >= msglen):
                break
            
        #Decode string of base64 
        img_data = full_msg[HEADERSIZE:]

        img_data = base64.b64decode(img_data)
        #Get byte object from string
        #Convert byes

        img_data = np.fromstring(img_data,dtype=np.uint8)
        
        #Reshape
        img_data = np.reshape(img_data,(128,128,3))



        
        img_data = tf.image.convert_image_dtype(img_data,dtype=tf.float32,saturate=False)
        
        res = loaded_model.predict(np.array([np.array(img_data)]))[0]
        #take the person layer
        res = res[:,:,1]
        #binarize the mask
        res[res>=0.1] = 1
        res[res<0.1] = 0
        
        #Cast mask as uint8
        res  = res.astype('uint8')
        
        #Convert image to string
        #cv2.imshow("Image",np.array(res))
        #cv2.waitKey(0)
        
        
        img_str = base64.b64encode(res)
        
        #print(img_str)
        
        #Send image as string to client
        img_str = f'{len(img_str):<{HEADERSIZE}}'+img_str.decode("utf-8")

        #print(img_str)
        #time.sleep(5)
        clientsocket.sendall(bytes(img_str,"utf-8"))


if __name__ == "__main__":
    print(sys.argv[0:])
    print(sys.argv[1])
    print(sys.argv[2])
    print(sys.argv[3])
    loaded_model = model_load(sys.argv[3])
    s = start_server(sys.argv[1],int(sys.argv[2]))
    while True:
        main(loaded_model,s,sys.argv[1],int(sys.argv[2]))
        print("Connection was closed")
    
