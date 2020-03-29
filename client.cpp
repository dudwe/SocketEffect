// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "base64.h"
#include <iostream>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std;

int main(int argc, char const *argv[])
{
  int sock,valread;
  int i = 0;
  struct sockaddr_in server;
  char server_reply[4096] = {NULL};

  //Create socket
  sock = socket(AF_INET,SOCK_STREAM,0);
  if(sock == -1){
    printf("Could not create socket\n");
  }

  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_family = AF_INET;
  server.sin_port = htons(8080);

  //Connect to remote server

  if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0){
    perror("connect failed. Error");
  }else{
    printf("Connection established!");

    //Read in a sample image
    Mat image;
    image = imread("greggs.jpg",CV_LOAD_IMAGE_COLOR); //read some file
    
    //Do arbritrary prepocessing on image
    resize(image,image,cv::Size(128,128));

    //Convert to a base64 compatable format
    string encoded_png;

    uchar *buf = image.data;
    auto base64_png = reinterpret_cast<const unsigned char*>(image.data);
    encoded_png = base64_encode(base64_png, image.total()*3);
    printf("Length of data is %d\n",encoded_png.length());
    int size = encoded_png.length();
    
    //Add length as header with spaces
    string header =to_string(size); 
    int spaces = header.length();
    for(int i = spaces; i< 10;i++){
      header+= ' ';
    }
    printf("Header is %s :\n",header.c_str());

    //Add the header to head of the encoded_png string
    encoded_png = header+encoded_png;
    //Send this accross
    send(sock,encoded_png.c_str(),encoded_png.length(),0);

    string full_msg ;
    bool new_msg = true;
    int msglen;
    while(true){
      //Empty the buffer
      server_reply[4096] = {NULL};      
      //Recv data into buffer
      valread = read(sock,server_reply,4096);
      //Display the buffer contents
      if(new_msg){
	string packet_header;
	for(i=0;i<10;i++){
	  if(server_reply[i]==' '){
	    break;
	  }
	  packet_header+=server_reply[i];
	}
	//Extract the head of the packet

	printf("RECV Packet Header is %s \n",packet_header.c_str());
	msglen = stoi(packet_header);
	new_msg = false;
      }
      full_msg+=server_reply;
      if(full_msg.length()-10>=msglen){
	printf("Received image from server\n");
	break;
      }
    }

    //Trim the header
    full_msg = full_msg.erase(0,10);

    //Trim the end
    full_msg = full_msg.substr(0,msglen);
        
    //Decode image to byte string
    std::string decoded_data = base64_decode(full_msg);

    //Convert to uchar vector
    std::vector<uchar> data(decoded_data.begin(), decoded_data.end());

    //Cast this to  matrix
    Mat recv_img(128, 128, CV_8UC1, data.data());
            
    namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
    imshow( "Display window", recv_img );                   // Show our image inside it.

    waitKey(0); 
  }
  close(sock);
  return 0;
} 
