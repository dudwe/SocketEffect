#include "VideoEffect.h"
//#include "pca/IPCA.hpp"

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv4/opencv2/tracking/tracker.hpp>
#include <deque>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <typeinfo>

using namespace std;
using namespace cv;
//using namespace Eigen;

class FaceBlurring : public VideoEffect {
private:
  const string PATH_TO_FACE_DETECTOR = "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt2.xml";
  const size_t MAX_FACE_COUNT = 5;
  const float SUBSAMPLE_SCALE = 4;

  CascadeClassifier cascade;
  unordered_map<int, Ptr<TrackerKCF>> trackers;
  unordered_map<int, int> framesSinceFailure;
  unordered_map<int, Rect2d> faces;
  unordered_set<int> newFaceIndex;
  int frameCount;
  //unordered_map<int, VectorXf> prev_face;
  //float inter_class_dist;

  bool facesIsEmpty() {
    return index.empty();
  }

  bool rectIntersect(Rect& r, Rect2d& r2) {
    bool x = max((double) r.x, (double) r2.x) < min((double) r.x+(double) r.width,(double) r2.x+(double) r2.width);
    bool y = max((double) r.y, (double) r2.y) < min((double) r.y+(double) r.height,(double) r2.y+(double) r2.height);
    return x && y;
  }

  double dimDiff(Rect& r, Rect2d& r2) {
    return max((double) r.width, (double) r2.width)/min((double) r.width, (double) r2.width);
  }

  void detectFaces(const Mat& frame, const Mat& gray) {

    TrackerKCF::Params params;
    params.detect_thresh = 0.25;

    vector<Rect> rectFaces;

    Mat smallImg;

    scaleDownFrame(gray, smallImg);
    cascade.detectMultiScale(smallImg, rectFaces);

    resizeRect(rectFaces);

    for (size_t i = 0; i < rectFaces.size(); i++) {
      newFaceIndex.insert(i);
      vector<int> proposal;
      //for (size_t j = 0; j < faces.size(); j++) {
      for (int j : index) {
	if (rectIntersect(rectFaces[i], faces[j])) {
	  if (dimDiff(rectFaces[i], faces[j]) > 1.2) {
	    proposal.push_back(j);
	  }
	  newFaceIndex.erase(i);
	}
      }

      if (proposal.size() == 1) {
	faces[proposal[0]] = Rect2d(rectFaces[i]);
	trackers[proposal[0]] = TrackerKCF::create(params);
	trackers[proposal[0]]->init(frame, faces[proposal[0]]);
      }
    }
    if (!newFaceIndex.empty()) {
      auto it = newFaceIndex.begin();
      for (int i = 0; i < MAX_FACE_COUNT && it != newFaceIndex.end(); i++) {
	if (index.find(i) != index.end()) {
	  continue;
	}
	index.insert(i);
	framesSinceFailure[i] = 0;
	faces[i] = Rect2d(rectFaces[*it]);
	trackers[i] = TrackerKCF::create(params);
	trackers[i]->init(frame, faces[i]);
	it++;
      }
    }
    newFaceIndex.clear();
  }


  void updateTrackers(const Mat& frame, const Mat& gray) {

    if (frameCount == 0) {
      detectFaces(frame, gray);
    } else {
      for (int i : index) {
	if (trackers[i]->update(frame, faces[i])) {
	  framesSinceFailure[i] = 0;
	} else if (framesSinceFailure[i] > 2) {
	  index.erase(i);
	} else {
	  framesSinceFailure[i]++;
	}
      }
    }
    frameCount = (frameCount + 1) % 5;

  }

  void displayFaces(Mat& frame, unordered_map<int, Rect2d>& facesToDisplay) {
    Scalar color = Scalar(0, 255, 0);
    for (int i : index) {
      rectangle(frame, facesToDisplay[i], color, 5, 8);
    }

  }


  void scaleDownFrame(const Mat& bigFrame, Mat& smallFrame) {
    double scale = 1 / SUBSAMPLE_SCALE;
    resize(bigFrame, smallFrame, Size(), scale, scale, INTER_LINEAR);
  }

  void resizeRect(vector<Rect>& r) {
    for (size_t i = 0; i < r.size(); i++) {
      r[i].height *= SUBSAMPLE_SCALE;
      r[i].width *= SUBSAMPLE_SCALE;
      r[i].x *= SUBSAMPLE_SCALE;
      r[i].y *= SUBSAMPLE_SCALE;
    }
  }
public:

  unordered_map<int, Rect2d> facesScaled;
  unordered_set<int> index;

  FaceBlurring() {
    cascade.load(PATH_TO_FACE_DETECTOR);
    frameCount = 0;
  }

  void processFrame(const SourceFrame& frame) override {
    updateTrackers(frame.frame, frame.grayscale_frame);
  }

  void applyEffect(const SourceFrame& original, Mat& frame) override {
    facesScaling(original, frame);
    displayFaces(frame, facesScaled);
  }

  void facesScaling(const SourceFrame& original, Mat& frame) {
    int src_width = static_cast<int>(original.frame.cols);
    int src_height = static_cast<int>(original.frame.rows);

    int frame_width = static_cast<int>(frame.cols);
    int frame_height = static_cast<int>(frame.rows);

    float x_scale = (float) frame_width / (float) src_width;
    float y_scale = (float) frame_height / (float) src_height;


    for (int i : index) {
      facesScaled[i] = faces[i];
      facesScaled[i].height *= y_scale;
      facesScaled[i].width *= x_scale;
      facesScaled[i].x *= x_scale;
      facesScaled[i].y *= y_scale;
    }
  }

  unordered_map<int, Rect2d>* getFaces(const SourceFrame& original, Mat& frame) {
    facesScaling(original, frame);
    return &facesScaled;
  }

  unordered_set<int>* getIndex() {
    return &index;
  }

};
