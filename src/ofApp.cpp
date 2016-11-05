#include "ofApp.h"

#define FADE_TIME 2.0
#define VIDEO_DIR "/Users/grant/Documents/Processing/lovescompany/data/videos/GOODDARK"//"videos"
#define DEBUG_ORDER false
#define DEBUG_WITH_MOUSE true
#define PRELOAD false

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(0,0,0);
  // XXX: Nobody knows what this does?!?!
	ofSetVerticalSync(true);

	dir.listDir(VIDEO_DIR);
  // Linux doesn't give us a sorted list
	dir.sort();
  if (PRELOAD) {
    cout << "loading " << dir.size() << " videos...\n";
    for(int i = 0; i < (int)dir.size(); i++){
      // TODO: if not equal .DS_Store? use a filter for just video files??
      // TODO: if duration <= 2*FADE_TIME, remove this from the list and stuff
      videos.push_back(ofVideoPlayer());
      loadVideo(i, videos[i]);
    }
    cout << "...done.\n";
  }

  curVideoIndex = 0;
  current = nextVideo();
  next = NULL;

  opcClient.setup("127.0.0.1", 7890);
}

void ofApp::loadVideo(int dirIndex, ofVideoPlayer& videoObj) {
  cout << "loading " << dir.getPath(dirIndex) << "\n";
  videoObj.load(dir.getPath(dirIndex));
  videoObj.setLoopState(OF_LOOP_NONE);
  videoObj.play();
  videoObj.setVolume(0);
  videoObj.setPaused(true);
}

ofVideoPlayer* ofApp::nextVideo() {
  // Returns a pointer to a video that should be set as "next":
  // it is not-current, loaded, and ready to un-pause.
  ofVideoPlayer* m = NULL;
  int numVideos = PRELOAD ? videos.size() : dir.size();
  do {
    if (!DEBUG_ORDER) {
      curVideoIndex = ofRandom(numVideos);
    }

    if (PRELOAD) {
      m = &videos[curVideoIndex];
    } else {
      m = new ofVideoPlayer();
      loadVideo(curVideoIndex, *m);
    }
    if (!m->isLoaded()) {
      if (!PRELOAD) {
        delete m;
      }
      m = NULL;
    }

    if (DEBUG_ORDER) {
      curVideoIndex = (curVideoIndex + 1) % numVideos;
    }
  // next and current shouldn't be the same (if PRELOAD); NULL means the
  // video we tried in this loop didn't work for some reason.
  } while (m == current || m == NULL);
  //cout << "next is " << m->getVideoPath() << " length " << m->getDuration()
  //    << '\n';
  // Start the new video
  m->setPaused(false);
  return m;
}

void ofApp::draw(){
  //XXX
	ofBackground(0,0,0);
  float timeLeft = current->getDuration() * (1.0 - current->getPosition());
  //cout << timeLeft << ',' << current->getPosition() << ',' << current->getDuration() << ',' << current->getSpeed() << ',' << current->isPaused() << ',' << current->isPlaying() <<"!\n";
  // Video is over if it says it's no longer playing or if getPosition()
  // returns -inf.
  if ((!current->isPlaying() || isinf(timeLeft))
      // The above tests sometimes trigger falsely (when a video is starting??),
      // so only do this if we have a valid "next"
      && next != NULL) {
    // The current video is over; time set next as current.
    if (PRELOAD) {
      // Rewind video for the next time it comes up in the shuffle
      current->setPosition(0);
      // And pause it for playing later
      current->setPaused(true);
    } else {
      // Delete the old video object to save memory
      // TODO: Would it be better to reuse an existing object with a new
      // video? Or is that asking for memory leaks?
      delete current;
    }
    current = next;
    next = NULL;
    timeLeft = -1;
  } else if (next == NULL && timeLeft < FADE_TIME) {
    // It's time to start crossfading to the next video; do it!
    next = nextVideo();
  }

  ofSetColor(255, 255, 255, 255);
  current->draw(0, 0, ofGetWidth(), ofGetHeight());
  if (next != NULL) {
    // Fading in: draw next with increasing opacity to cover current
    ofSetColor(255, 255, 255, (1.0 - timeLeft / FADE_TIME) * 255.0);
    next->draw(0, 0, ofGetWidth(), ofGetHeight());
  }

  if (DEBUG_WITH_MOUSE) {
    ofSetColor(255, 255, 255, 255);
    ofDrawEllipse(ofGetMouseX(), ofGetMouseY(),
        ofGetWidth()/10.0, ofGetHeight()/10.0);
  }

  if (!opcClient.isConnected()) {
    // Will continue to try and reconnect to the Pixel Server
    opcClient.tryConnecting();
  } else {
    vector<vector<ofColor>> outputs;
    ofImage grab;
    grab.grabScreen(0, 0, 40, 10);
    vector<ofColor> curout;
    for (int panel = 0; panel < 8; panel++) {
      for (int zz = 0; zz < 5; zz++) {
        for (int height = 0; height < 10; height++) {
          curout.push_back(grab.getColor(panel*5 + zz + (zz % 2 ? 9-height : height)*40));
          if (curout.size() == 64) {
            outputs.push_back(curout);
            curout.clear();
          }
        }
      }
    }
    if (curout.size()) {
      outputs.push_back(curout);
    }
    for (int i = 0; i < outputs.size(); i++) {
      opcClient.writeChannel(i+1, outputs[i]);
    }
  }
}

void ofApp::update(){
  // Update any videos that we have.
  if (current) {
    current->update();
  }
  if (next) {
    next->update();
  }
}

void ofApp::mousePressed(int x, int y, int button) {
  // Only skip if we're playing a video and aren't currently in the middle
  // of an existing transition.
  if (current != NULL && next == NULL) {
    // Skip to crossfade to next by jumping towards the end of the current video
    // (but not earlier than zero, of course)
    // NOTE: Some videos seem to get stuck at the seeked position for a while
    // before they start playing again: something about the encoding makes
    // it hard to seek quickly?
    current->setPosition(MAX(0, 1.0 - FADE_TIME / current->getDuration()));
  }
}


/*
  // Connect to the local instance of fcserver. You can change this line to connect to another computer's fcserver
  opc = new OPC(this, "127.0.0.1", 7890);
  opc.setStatusLed(true);
  for (int i = 0; i < 8; i++) {
    //opc.ledGrid(i*50, HEIGHT, 5, width * (0.5 + i) / 8, height/2, width/WIDTH, height/HEIGHT, 3*HALF_PI, true, true);
    opc.ledGrid((7-i)*50, HEIGHT, 5, width * (0.5 + i) / 8, height/2, width/WIDTH, height/HEIGHT, HALF_PI, true, true);
  }
*/
