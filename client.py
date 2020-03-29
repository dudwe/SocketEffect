import socket

HEADERSIZE = 10

s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

s.connect((socket.gethostname(),1234))






while True:

    full_msg = ''
    new_msg = True


    #Send data to server
    test = "Hello from client"
    test = f'{len(test):<{HEADERSIZE}}'+test
    
    s.send(bytes(test,"utf-8"))
    #recv data from server

    while True:
        msg = s.recv(16)
        if(new_msg):
            print(f"new message length: {msg[:HEADERSIZE]}")
            msglen = int(msg[:HEADERSIZE])
            new_msg = False
            
        full_msg += msg.decode("utf-8")
        if(len(full_msg)-HEADERSIZE == msglen):
            print("full msg recvd")
            print(full_msg[HEADERSIZE:])
            break
    

