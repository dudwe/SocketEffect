//
// Created by Alexander Logan on 07/02/2020.
//

#ifndef RNG_STREAMING_BACKEND__EXAMPLEEFFECTS_H_
#define RNG_STREAMING_BACKEND__EXAMPLEEFFECTS_H_

#include "opencv2/objdetect.hpp"


#include "VideoEffect.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "base64.h"

#define FRAME_SIZE 128
#define FRAME_SKIP 5
#define BUFFER_SIZE 4096

using namespace cv;
using namespace std;

class PrivacyFilterEffect : public VideoEffect {
  //Establish Conneciton with python server

  //We have 2 segnet servers
  
  int valread;
  array<int,2> sock;
  
  int i = 0;

  array<struct sockaddr_in,2> server;
  //struct sockaddr_in server;

  
  int frame_number;
  Mat mask;
  int distance =0;

 public:
  void effectEnabled(){
    frame_number = 0;
    printf("EFFECT ENEABLED\n");

    //Create socket
    for(i=0;i<sock.size();i++){
      sock[i] = socket(AF_INET,SOCK_STREAM,0);
      if(sock[i]==-1){
	printf("Could not create socket\n");      
      }      
    }
    
    server[0].sin_addr.s_addr = inet_addr("127.0.0.1");
    server[0].sin_family = AF_INET;
    server[0].sin_port = htons(9090);

    server[1].sin_addr.s_addr = inet_addr("127.0.0.1");
    server[1].sin_family = AF_INET;
    server[1].sin_port = htons(8080);



    
    for(i=0;i<sock.size();i++){
      //Connect to server
      if (connect(sock[i] , (struct sockaddr *)&server[i] , sizeof(server[i])) < 0){
	perror("connect failed. Error");
      }else{
	printf("Connection established!");
      }
    }
  }
  void effectDisabled(){
    //Close connection with python server
    printf("Effect Disabled! Closing socket connections\n");
    for(i=0;i<sock.size();i++){
      close(sock[i]);
    }
  }


  int sock_loc = 0;  
  void applyEffect(const SourceFrame& original, cv::Mat& frame) override
  {
    
    //printf("Received image from server Frame Number %d\n",frame_number);
    if(frame_number % FRAME_SKIP == 0){
      /*
      if(frame_number % 2 == 0){
	sock_loc = 0;
      }else{
	sock_loc = 1;
      }
      */
      //send data to the server
      Mat send_frame;

      //Resize the frame to 128 by 128
      resize(frame,send_frame,cv::Size(FRAME_SIZE,FRAME_SIZE));

      //Convert frame to base64 format
      string encoded_string;
      
      uchar *send_buffer = send_frame.data;
      auto base64_send = reinterpret_cast<const unsigned char*>(send_frame.data);
      encoded_string = base64_encode(base64_send,send_frame.total() *3);
    
      //Add header to the string with padding
      int size = encoded_string.length();
      string header = to_string(size);
      int spaces = header.length();
      for(int i = spaces;i<10;i++){
	header+=' ';
      }
      
      //Add header to the data
      encoded_string = header + encoded_string;
      
      //Send image to python server
      send(sock[sock_loc],encoded_string.c_str(),encoded_string.length(),0);

      //Receive mask and apply to image
      string recv_msg;
      bool new_msg = true;
      int msglen;
      int current_length=0;
      while(true){
	char recv_buffer[BUFFER_SIZE];
	//Empty the buffer
	//Recv data into the buffer
	valread = read(sock[sock_loc],recv_buffer,BUFFER_SIZE);
	
	//Extract header if first packet
	if(new_msg){
	  string packet_header;
	  for(i=0;i<10;i++){
	    if(recv_buffer[i]==' '){
	      break;
	    }
	    packet_header+=recv_buffer[i];
	    
	  }     
	  msglen = stoi(packet_header);
	  new_msg = false;
	  //Trim header from buffer and store remainder in string
	  for(i=10;i<BUFFER_SIZE;i++){
	    recv_msg+=recv_buffer[i];
	    current_length++;
	  }

	}

	else if(current_length+4096>msglen){
	  //Last packet so we trim 
	  for(i=0;i<msglen-current_length;i++){
	    recv_msg+=recv_buffer[i];
	  }
	  current_length=current_length + msglen-current_length;
	  break;
	}
	else{
	  for(i=0;i<BUFFER_SIZE;i++){
	    recv_msg+=recv_buffer[i];
	    current_length++;
	  }

	}
      }

      //Decode image to byte string
      std::string decoded_data = base64_decode(recv_msg);

      //Convert to uchar vector
      std::vector<uchar> data(decoded_data.begin(), decoded_data.end());
      //Cast this to  matrix
      Mat recv_img(128, 128, CV_8UC1, data.data());

      //Estimate distance based on number of pixels in mask
      double sum_val = sum(recv_img)[0];
      if(sum_val/(128*128) < 0.4){
	//Use general segnet
	sock_loc = 1;
	printf("Use far\n");
      }else{
	//use close range segnet
	sock_loc = 0;
	printf("Use close close\n");
      }
      

      //Resize mask to video dimensions
      resize(recv_img,mask,cv::Size(1280,720));
      
      //Post Process the mask
      //Erode the mask and dilate with a 10x10 kernel
      //Erode
      erode(mask,mask, getStructuringElement(MORPH_RECT, Size(10, 10)));
      //Dilate
      dilate(mask,mask, getStructuringElement(MORPH_RECT, Size(15, 15)));
      //Fill Holes in mask
      morphologyEx(mask,mask,MORPH_CLOSE,getStructuringElement(MORPH_RECT, Size(50, 50)));
      //Remove artifacts
      morphologyEx(mask,mask,MORPH_OPEN,getStructuringElement(MORPH_RECT, Size(50, 50)));
    }
    //Apply mask as blurring mask to the video
    Mat blurMask;
    GaussianBlur(frame,blurMask,Size(25,25),1000,1000);

    //Invert the mask
    Mat mask_inv;
    bitwise_not(255*mask,mask_inv);
    
    blurMask.copyTo(frame,mask_inv);

    frame_number++;
    
  }  
};


class RaiseBlackEffect : public VideoEffect {
 public:
  explicit RaiseBlackEffect(uchar level)
  {
    generateLUT(level);
  }

  void applyEffect(const SourceFrame& original, cv::Mat& frame) override
  {
    cv::LUT(frame, lut, frame);
  }

  bool receiveParameter(std::string&& param, std::string&& value) override
  {
    if (param == "level") {
      try {
	uchar lvl = std::stoi(value);
	generateLUT(lvl);
	return true;
      } catch (std::invalid_argument& _) {
	return false;
      }
    }

    return false;
  }

 private:
  cv::Mat lut;

  void generateLUT(uchar level)
  {
    lut = cv::Mat(1, 256, cv::DataType<uchar>().type);
    uchar* lutPtr = lut.ptr();
    for (int i = 0; i < 256; i++) {
      lutPtr[i] = i >= level ? (uchar)i : level;
    }
  }
};

class EqualiseLuminanceEffect : public VideoEffect {
 public:
  void applyEffect(const SourceFrame& original, cv::Mat& frame) override
  {
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    cv::split(ycrcb, channels);
    cv::equalizeHist(channels[0], channels[0]);
    cv::merge(channels, ycrcb);
    cv::cvtColor(ycrcb, frame, cv::COLOR_YCrCb2BGR);
  }

 private:
  cv::Mat ycrcb;
  std::vector<cv::Mat> channels = std::vector<cv::Mat>(3);
};

#endif //RNG_STREAMING_BACKEND__EXAMPLEEFFECTS_H_
