import socket




HEADERSIZE = 10

s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
s.bind((socket.gethostname(),1234))
s.listen(5)



while True:
    #Move conn up for continous stream
    clientsocket,address = s.accept()
    print(f"Connection from {address} has been established")

    #Recv data from server
    full_msg = ''
    new_msg = True

    while True:
        msg = clientsocket.recv(16)
        if(new_msg):
            print(f"Client message length: {msg[:HEADERSIZE]}")
            msglen = int(msg[:HEADERSIZE])
            new_msg = False
        full_msg += msg.decode("utf-8")
        if(len(full_msg)-HEADERSIZE == msglen):
            print("Received data from client")
            print(full_msg[HEADERSIZE:])
            break
            
    

    #Send data to server

    msg = "Welcome to the server"
    msg = f'{len(msg):<{HEADERSIZE}}'+msg

    
    clientsocket.send(bytes(msg,"utf-8"))
    

