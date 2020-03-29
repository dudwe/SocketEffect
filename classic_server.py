import socket
import sys
import numpy as np
import cv2



def recvall(sock):
    BUFF_SIZE = 49152 # 4 KiB
    data = b''
    while True:
        part = sock.recv(BUFF_SIZE)
        data += part
        print(len(part))
        if len(part) < BUFF_SIZE:
            # either 0 or end of data
            break
    return data


def main():

    #Create  a socket
    sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

    #bind socket to the port
    server_address = ('localhost',8888)
    sock.bind(server_address)

    #Listen for a connection
    sock.listen(1)

    imgSize = 128*128*3
    print(imgSize)

    clientsocket,address = sock.accept()
    while True:

        print(f"Connection from {address} has been established")


        #Receive image from client
        data = recvall(clientsocket)
        #data = clientsocket.recv(imgSize)

        if not data:
            print("ERROR")


        #The data received is in bytes, it must be converted 
        data = np.fromstring(data,dtype='uint8')
    
        #Receive data from client
  
        #do some arbritrary processing
        data = np.reshape(data,(128,128,3))

        
    
        #convert image to grayscale
        img_to_send = cv2.cvtColor(data,cv2.COLOR_BGR2GRAY)
        #Show a cv2 image
        cv2.imshow('mod_client',img_to_send)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
        
        #send the data back to the server
        print("sending some data")
        clientsocket.sendall("Data from server");
        print("data sent")
main()
