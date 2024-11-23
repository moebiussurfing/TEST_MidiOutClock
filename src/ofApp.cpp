#include "ofApp.h"

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofBackground(255);
	ofSetLogLevel(OF_LOG_VERBOSE);

	// print the available output ports to the console
	midiOut.listOutPorts();

	// connect
	midiOut.openPort(1); // by number

	channel = 1;
	note = 0;
	velocity = 0;

	clockThread.setBpm(120.0f);
	clockThread.setup(&midiOut);
}

void ofApp::update() {
}

void ofApp::draw() {
	ofSetColor(0);
	stringstream text;
	text << endl << endl
		<< "MIDI OUT CLOCK" << endl << endl
		<< "CONNECTED PORT " << midiOut.getPort() << endl
		<< "\"" << midiOut.getName() << "\"" << endl << endl
		<< "KEYS" << endl
		<< "Space:      Start/Stop" << endl
		<< "Up/Down:    Tempo +/-" << endl
		<< "            MouseDrag y" << endl
		<< "BackSpace:  Reset" << endl << endl
		<< "CLOCK:      " << (clockThread.isClockRunning() ? "ON" : "OFF") << endl
		<< "BPM:        " << ofToString(clockThread.getBpm(), 2) << endl << endl << endl
		<< "TEST NOTES" << endl
		<< "MousePress x" << endl
		<< "Note:       " << note << endl
		<< "Velocity:   " << velocity;
	ofDrawBitmapString(text.str(), 20, 20);

	if (ofGetElapsedTimeMillis() % 2000 == 0)
		ofLogNotice() << "MIDI Clock BPM: " << clockThread.getBpm();
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case ' ': // Start/Stop MIDI clock
		clockThread.toggleClock();
		break;

	case OF_KEY_UP: // Increase tempo
		clockThread.setBpm(clockThread.getBpm() + 1.0f);
		break;

	case OF_KEY_DOWN: // Decrease tempo
		clockThread.setBpm(clockThread.getBpm() - 1.0f);
		break;

	case OF_KEY_BACKSPACE: // Reset
		clockThread.setBpm(120.0f);
		break;
	}
}

void ofApp::mousePressed(int x, int y, int button) {
	// Trigs a random noteOn + noteOff
	int note_ = ofMap(x, 0, ofGetWidth(), 0, 127);
	int velocity_ = ofRandom(32, 128);
	midiOut.sendNoteOn(channel, note_, velocity_);

	note = note_;
	velocity = velocity_;

	velocity_ = 0;
	midiOut << NoteOff(channel, note_, velocity_);
}

void ofApp::mouseDragged(int x, int y, int button) {
	// Set bpm
	float v = ofMap(x, 0, ofGetHeight(), 400, 10);
	clockThread.setBpm(v);
}

void ofApp::exit() {
	clockThread.stop();
	midiOut.closePort();
}
