
#include "ExampleEffects.h"
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

class ROIPrivacyFilterEffect : public VideoEffect {
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

  int sock_loc = 0;//determines which thing we send to
  Mat global_mask;
  
 public:

  std::unique_ptr<FaceBlurring> tracker;
  
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

    
    //Portrait
    server[0].sin_addr.s_addr = inet_addr("127.0.0.1");
    server[0].sin_family = AF_INET;
    server[0].sin_port = htons(9090);

    //Med and long rangfe
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

    //Init the Face Tracker
    tracker = std::make_unique<FaceBlurring>();

    //Init counter
    frame_number = 0;


  }
  
  void effectDisabled(){
    //Close connection with python server
    printf("Effect Disabled! Closing socket connections\n");
    for(i=0;i<sock.size();i++){
      close(sock[i]);
    }
  }



  void applyEffect(const SourceFrame& original, cv::Mat& frame) override
  {

    tracker->processFrame(original);
    tracker->facesScaling(original,frame);
    
    if(frame_number % FRAME_SKIP == 0){

      
      Scalar color = Scalar(0, 255, 0);
      

      
      //Init the global maskm
      global_mask = Mat::zeros(cv::Size(1280, 720), CV_8UC1);
      for (int i : tracker->index) {

	Rect current_bbox = tracker->facesScaled[i];
	//rectangle(frame, tracker->facesScaled[i], color, 5, 8);
	Mat roi = frame(current_bbox);
      
	//Get the size of the roi
	double roi_size = roi.rows * roi.cols;

	//Generate ROI bbox
	Rect roi_bbox;
	if(roi_size/(frame.rows * frame.cols) < 0.05){
	  printf("Far away \n");
	  sock_loc = 1;
	  roi_bbox = extendBox(current_bbox,1);
	}else{
	  printf("Close \n");
	  sock_loc = 0;
	  roi_bbox = extendBox(current_bbox,0);
	}




	//Extract the roi
	Mat expand_roi = frame(roi_bbox);

	//Get segmentation mask for ROI
	Mat recv_img = sendAndReceive(expand_roi,sock_loc);


	//Debug rect
	rectangle(frame, roi_bbox, Scalar(255,0,0), 5, 8);

	//Copy the segmask
	recv_img.copyTo(global_mask(roi_bbox));
      }
    }
    //Apply gaussian blur to frame
    Mat blurMask;
    GaussianBlur(frame,blurMask,Size(25,25),1000,1000);

    //Invert the mask
    Mat mask_inv;
    bitwise_not(255*global_mask,mask_inv);
    
    blurMask.copyTo(frame,mask_inv);
    frame_number++;
  }

  Rect extendBox(cv::Rect &bbox,int scale_factor){
    //Move the box up by half a height
    bbox = bbox - Point(0,bbox.height/2);

    if(scale_factor==0){
      //Move the box up by half a height
      bbox = bbox - Point(0,bbox.height/2);
      //Shift box left by 1.5 width
      bbox = bbox - Point(bbox.width + bbox.width/2,0);
      //Expand box by factor of 3.5
      bbox += cv::Size(bbox.width*3.5,bbox.height*3.5);
    }else{
      //Shift box left by 2.5 width
      bbox = bbox - Point((2*bbox.width) + bbox.width/2,0);
      //Expand box by factor of 6.5
      bbox += cv::Size(bbox.width*6.5,bbox.height*6.5);
    }
    //Is the top left coord in frame?
    //is the bot right coord in frame?
    // cout << "old_coord tl" << bbox.tl().x << "," << bbox.tl().y << "\n";
    //cout << "old coord br" <<  bbox.br().x << ","<< bbox.br().y << "\n";
    

    //We create a new set of points which are in bounds
    Point new_tl;
    if(bbox.tl().x<0){
      new_tl.x = 0;
    }else if(bbox.tl().x>1280){
      new_tl.x = 1280;
    }else{
      new_tl.x = bbox.tl().x;
    }
    
    if(bbox.tl().y<0){
      new_tl.y = 0;
    }else if (bbox.tl().y>720){
      new_tl.y = 720;
    }else{
      new_tl.y = bbox.tl().y;
    }

    //cout <<"New_cord tl "<< new_tl.x<<"," << new_tl.y << "\n";

    Point new_br;
    if(bbox.br().x<0){
      new_br.x = 0;
    }else if(bbox.br().x>1280){
      new_br.x = 1280;
    }else{
      new_br.x = bbox.br().x;
    }
    
    if(bbox.br().y<0){
      new_br.y = 0;
    }else if (bbox.br().y>720){
      new_br.y = 720;
    }else{
      new_br.y = bbox.br().y;
    }
    //cout <<"New_cord br "<< new_br.x<<"," << new_br.y << "\n";

    return Rect(new_tl,new_br);
  }


  Mat sendAndReceive(Mat roi,int sock_loc){
          //Resize the frame to 128 by 128
    Mat send_frame;
    resize(roi,send_frame,cv::Size(FRAME_SIZE,FRAME_SIZE));
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
    //Reconstruct the mask

    //Decode image to byte string
    std::string decoded_data = base64_decode(recv_msg);
    
    //Convert to uchar vector
    std::vector<uchar> data(decoded_data.begin(), decoded_data.end());

    Mat recv_img(128, 128, CV_8UC1, data.data());

    
    //Resize the mask
    resize(recv_img,recv_img,cv::Size(roi.cols,roi.rows));
    
    //Morph Processing
    morph_process(recv_img,10,15,50,50);


    //Apply Mask to the super mask
    
    //return the mask
    /*
    namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
    imshow( "Display window", recv_img *255  );                   // Show our image inside it.
    waitKey(0);*/
    return recv_img;
  }

  void morph_process(Mat &seg_mask,int erode_size,int dilate_size,int close_size, int open_size){
    //Erode
    erode(seg_mask,seg_mask, getStructuringElement(MORPH_RECT, Size(erode_size, erode_size)));
    //Dilate
    dilate(seg_mask,seg_mask, getStructuringElement(MORPH_RECT, Size(dilate_size, dilate_size)));
    //Fill Holes in mask
    morphologyEx(seg_mask,seg_mask,MORPH_CLOSE,getStructuringElement(MORPH_RECT, Size(close_size, close_size)));
    //Remove artifacts
    morphologyEx(seg_mask,seg_mask,MORPH_OPEN,getStructuringElement(MORPH_RECT, Size(open_size,open_size)));
  }

};



