#include "ofApp.h"

#define FADE_TIME 2.0
#define VIDEO_DIR "videos"
#define DEBUG_ORDER false
#define DEBUG_WITH_MOUSE false
#define PRELOAD false
#define PRINT_FRAMERATE true

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(0,0,0);
  // Very little documentation on here, but "vsync" seems to synchronize
  // the framerate with the update frequency of the monitor. We have no monitor,
  // so there's no need to do that.
	ofSetVerticalSync(false);
  // ...instead, give ourselves an explicit framerate.
  ofSetFrameRate(30);

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
  lastTimeLeft = -1;
  runningProgress = -1;

  opcClient.setup("127.0.0.1", 7890);
}

void ofApp::loadVideo(int dirIndex, ofVideoPlayer& videoObj) {
  cout << "loading " << dir.getPath(dirIndex) << "\n";
  if (PRELOAD) {
    videoObj.load(dir.getPath(dirIndex));
  } else {
    // TODO: Hard to believe this doesn't need more handling,
    // but it seems to just work and avoid blocking for a half-second
    // when starting a new video!
    videoObj.loadAsync(dir.getPath(dirIndex));
  }
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
  float timeLeft = current->getDuration() * (1.0 - current->getPosition());
  // Annoying complexity to save us from videos that get stuck near their end
  // without stopping: track lastTimeLeft to track how much the video has
  // progressed since last draw(). However, sometimes the video hasn't
  // at all simply because we're draw()ing too fast for its frames. So instead
  // of a simple timeLeft == lastTimeLeft, accumulate a runningProgress with
  // lastTimeLeft - timeLeft. That value can be zero for a few draw()s,
  // but after too long runningProgress dips below our threshold and we decide
  // the video is stuck and we should move on.
  if (lastTimeLeft >= 0) {
    if (runningProgress >= 0) {
      runningProgress = (runningProgress + lastTimeLeft - timeLeft) / 2.0;
    } else {
      runningProgress = lastTimeLeft - timeLeft;
    }
  }
  lastTimeLeft = timeLeft;
  // Video is over if it says it's no longer playing, or if getPosition()
  // returns -inf, or if timeLeft is really small,
  if ((!current->isPlaying() || isinf(timeLeft) || timeLeft < 0.001 ||
        // or if the video's position hasn't advanced in a while (see above).
        (runningProgress >= 0 && runningProgress < 0.001))
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
      current->close();
      delete current;
    }
    current = next;
    next = NULL;
    timeLeft = -1;
    lastTimeLeft = -1;
    runningProgress = -1;
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
    cout << "Not connected!\n";
  } else {
    // TODO: ofxOPC is kinda silly: it uses 8 64-pixel "channels" to arbitrarily
    // divide the first 512 pixels of the standard OPC address space and ignores
    // the rest. Work around this for now by generating 64-pixel output vectors
    // and sending them for each "channel", though our strands are 50 pixels.
    vector<vector<ofColor>> outputs;
    ofImage grab;
    grab.grabScreen(0, 0, 40, 10);
    vector<ofColor> curout;
    for (int panel = 0; panel < 8; panel++) {
      for (int zz = 0; zz < 5; zz++) {
        for (int height = 0; height < 10; height++) {
          curout.push_back(grab.getColor(panel*5 + zz,
              zz % 2 ? 9-height : height));
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
  
  if (PRINT_FRAMERATE && ofGetFrameNum() % 10 == 0) {
    cout << "FPS " << ofGetFrameRate() << " remain " << timeLeft
      << " pos " << current->getPosition() << " len " << current->getDuration()
      << " runningProgress " << runningProgress << "\n";
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

