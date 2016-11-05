#pragma once

#include "ofMain.h"
#include "ofxOPC.h"

class ofApp : public ofBaseApp{

	public:

		void setup();
		void update();
		void draw();
    void mousePressed(int x, int y, int button);

    ofVideoPlayer* nextVideo();
    void loadVideo(int dirIndex, ofVideoPlayer& videoObj);

    // The directory from which we load videos
		ofDirectory dir;
    // If PRELOAD is true, all loaded videos are stored here
		vector<ofVideoPlayer> videos;
    // Currently-playing / finishing video
    ofVideoPlayer* current;
    // Fading-in / next video
    ofVideoPlayer* next;
    // Current index into videos if DEBUG_ORDER is true
    int curVideoIndex;
    // Used to track video progress which can get stuck
    float lastTimeLeft;
    float runningProgress;

    ofxOPC opcClient;
};

