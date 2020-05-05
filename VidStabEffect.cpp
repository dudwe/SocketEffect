
#include "ExampleEffects.h"
#include "VideoEffect.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "base64.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>

#define FRAME_SIZE 128
#define FRAME_SKIP 5
#define BUFFER_SIZE 4096

using namespace cv;
using namespace std;


class VidStabEffect : public VideoEffect {
  const int SMOOTHING_RADIUS = 50;
  
 public:


  struct TransformParam{
    TransformParam(){}
    TransformParam(double _dx,double _dy,double _da){
      dx = _dx;
      dy = _dy;
      da = _da;
    }
    double dx,dy,da;

    void getTransform(Mat &T){
      // Reconstruct transformation matrix accordingly to new values
      T.at<double>(0,0) = cos(da);
      T.at<double>(0,1) = -sin(da);
      T.at<double>(1,0) = sin(da);
      T.at<double>(1,1) = cos(da);

      T.at<double>(0,2) = dx;
      T.at<double>(1,2) = dy;
      
    }
  };
  
  struct Trajectory
  {
    Trajectory() {}
    Trajectory(double _x, double _y, double _a) {
      x = _x;
      y = _y;
      a = _a;
    }
    
    double x;
    double y;
    double a; // angle
  };




  
  vector<Trajectory> cumsum(deque<TransformParam> &transforms)
  {
    vector <Trajectory> trajectory; // trajectory at all frames
    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;

    for(size_t i=0; i < transforms.size(); i++)
      {
	x += transforms[i].dx;
	y += transforms[i].dy;
	a += transforms[i].da;

	trajectory.push_back(Trajectory(x,y,a));

      }

    return trajectory;
  }

  vector <Trajectory> single_smooth(vector <Trajectory>& trajectory, int radius)
  {
    vector <Trajectory> smoothed_trajectory;

      double sum_x = 0;
      double sum_y = 0;
      double sum_a = 0;
      int frame_count = 0;

      for(int j=-radius; j <= count; j++) {
	if(count+j >= 0 && count+j < trajectory.size()) {
	  sum_x += trajectory[count+j].x;
	  sum_y += trajectory[count+j].y;
	  sum_a += trajectory[count+j].a;

	  frame_count++;
	}
      }

      double avg_a = sum_a / frame_count;
      double avg_x = sum_x / frame_count;
      double avg_y = sum_y / frame_count;

    smoothed_trajectory.push_back(Trajectory(avg_x, avg_y, avg_a));


    return smoothed_trajectory;
  }
  
  vector <Trajectory> smooth(vector <Trajectory>& trajectory, int radius)
  {
    vector <Trajectory> smoothed_trajectory;
    for(size_t i=0; i < trajectory.size(); i++) {
      double sum_x = 0;
      double sum_y = 0;
      double sum_a = 0;
      int count = 0;

      for(int j=-radius; j <= radius; j++) {
	if(i+j >= 0 && i+j < trajectory.size()) {
	  sum_x += trajectory[i+j].x;
	  sum_y += trajectory[i+j].y;
	  sum_a += trajectory[i+j].a;

	  count++;
	}
      }

      double avg_a = sum_a / count;
      double avg_x = sum_x / count;
      double avg_y = sum_y / count;

      smoothed_trajectory.push_back(Trajectory(avg_x, avg_y, avg_a));
    }

    return smoothed_trajectory;
  }

  void fixBorder(Mat &frame_stabilized)
  {
    Mat T = getRotationMatrix2D(Point2f(frame_stabilized.cols/2, frame_stabilized.rows/2), 0, 1.04);
    warpAffine(frame_stabilized, frame_stabilized, T, frame_stabilized.size());
  }
  
  void fixBorderNoScale(Mat &frame_stabilized)
  {
    Mat T = getRotationMatrix2D(Point2f(frame_stabilized.cols/2, frame_stabilized.rows/2), 0, 1);
    warpAffine(frame_stabilized, frame_stabilized, T, frame_stabilized.size());
  }

  
  void effectEnabled(){



  }
  
  void effectDisabled(){

  }


  Mat curr,curr_gray;
  Mat prev,prev_gray;
  int first =0;
  deque <TransformParam> transforms;
  Mat last_T;
  int count = 0;
  vector <TransformParam> transforms_smooth;
  Mat frame_avg;
  Mat stab_avg;
  double nsad = 0;

  void applyEffect(const SourceFrame& original, cv::Mat& frame) override
  {
    //Read frame
    if(first ==0){
      prev = frame;

      //Convert to grayscale
      cvtColor(prev,prev_gray,COLOR_BGR2GRAY);
      frame_avg = Mat::zeros(Size(1280,720), CV_32S);
      stab_avg = Mat::zeros(Size(1280,720), CV_32S);
      first = 1;
    }


    // Vector from previous and current feature points
    vector <Point2f> prev_pts, curr_pts;
    
    // Detect features in previous frame
    goodFeaturesToTrack(prev_gray, prev_pts, 200, 0.01, 30);

    //Get current frame
    curr = frame;

    //Convert current frame to grayscale
    cvtColor(curr, curr_gray, COLOR_BGR2GRAY);

    //Compute optical flow
    vector <uchar> status;
    vector <float> err;
    calcOpticalFlowPyrLK(prev_gray, curr_gray, prev_pts, curr_pts, status, err);

    //Remove bad points
    auto prev_it = prev_pts.begin();
    auto curr_it = curr_pts.begin();
    for(size_t k = 0; k < status.size(); k++)
      {
	if(status[k])
	  {
	    prev_it++;
	    curr_it++;
	  }
	else
	  {
	    prev_it = prev_pts.erase(prev_it);
	    curr_it = curr_pts.erase(curr_it);
	  }
      }
    
    // Find transformation matrix
    Mat T = estimateRigidTransform(prev_pts, curr_pts, false); 

    // In rare cases no transform is found.
    // We'll just use the last known good transform.
    if(T.data == NULL) last_T.copyTo(T);
    T.copyTo(last_T);

    // Extract traslation
    double dx = T.at<double>(0,2);
    double dy = T.at<double>(1,2);

    // Extract rotation angle
    double da = atan2(T.at<double>(1,0), T.at<double>(0,0));

    // Store transformation
    transforms.push_back(TransformParam(dx, dy, da));


    
    if(transforms.size()>SMOOTHING_RADIUS){
      transforms.pop_front();
    }

    


    if(count<100){

      //Mat tmp;
      //cvtColor(frame_stabilized,tmp,COLOR_BGR2GRAY);
      //add(stab_avg, tmp, stab_avg, cv::noArray(), CV_32S);
      //frame_avg = frame_avg + prev_gray;
    }


    
    //    cout << "Frame: " << count <<" Size of buffer "<< transforms.size() << " -  Tracked points : " << prev_pts.size() << endl;

    //Smooth the window of frames
    // Compute trajectory using cumulative sum of transformations
    vector <Trajectory> trajectory = cumsum(transforms);

    // Smooth trajectory using moving average filter
    vector <Trajectory> smoothed_trajectory = single_smooth(trajectory, SMOOTHING_RADIUS);

    
    vector <TransformParam> transforms_smooth;
    
    double diff_x = smoothed_trajectory[0].x - trajectory.back().x;
    double diff_y = smoothed_trajectory[0].y - trajectory.back().y;
    double diff_a = smoothed_trajectory[0].a - trajectory.back().a;

    // Calculate newer transformation array
    dx = transforms.back().dx + diff_x;
    dy = transforms.back().dy + diff_y;
    da = transforms.back().da + diff_a;

    transforms_smooth.push_back(TransformParam(dx, dy, da));
    
    Mat frame_stabilized;
    //apply transform to the current frame
    transforms_smooth[count];
    // Apply affine wrapping to the given frame
    warpAffine(frame, frame_stabilized, T, frame.size());
    // Scale image to remove black border artifact
    fixBorder(frame_stabilized);




    //Add the frame
    if(count<100){
      //Stab 
      Mat tmp;
      cvtColor(frame_stabilized,tmp,COLOR_BGR2GRAY);
      add(stab_avg, tmp, stab_avg, cv::noArray(), CV_32S);

      Mat tmp_nsad  =  Mat::zeros(Size(1280,720), CV_32S);
      tmp_nsad = tmp-prev_gray;

      //Normalize
      nsad = sum(tmp_nsad)[0];
      cout<<"Newbef: " <<nsad <<endl;      
      nsad = nsad /12808720;
      cout<<"New: " <<nsad <<endl;      
      //frame_avg = frame_avg + prev_gray;

      //Original
      tmp_nsad  =  Mat::zeros(Size(1280,720), CV_32S);
      tmp_nsad = curr_gray-prev_gray;
      //Normalize
      //normalize(tmp_nsad, tmp_nsad, 0, 1,NORM_MINMAX);
      nsad = sum(tmp_nsad)[0];
      cout<<"Oldbef: " <<nsad <<endl;      

      nsad = nsad /12808720;
      cout<<"Old: " <<nsad <<endl;      
      cout <<nsad <<endl;
      
      add(frame_avg, prev_gray, frame_avg, cv::noArray(), CV_32S);      
    }
    if(count==100){
      frame_avg = frame_avg/100;
      frame_avg.convertTo(frame_avg, CV_8UC1);
      namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
      imshow( "Display window", frame_avg );                   // Show our image inside it.

      waitKey(0);

      stab_avg = stab_avg/100;
      stab_avg.convertTo(stab_avg, CV_8UC1);
      namedWindow( "Stab avg", WINDOW_AUTOSIZE );// Create a window for display.
      imshow( "Stab avg", stab_avg );                   // Show our image inside it.
      waitKey(0);
    }

    frame = frame_stabilized;    
    count = count + 1;

    // Move to next frame
    curr_gray.copyTo(prev_gray);   
  }

};



