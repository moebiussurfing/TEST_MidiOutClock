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

	clockThread.setup(&midiOut);
}

void ofApp::update() {
}

void ofApp::exit() {
	clockThread.stop();
	midiOut.closePort();
}

void ofApp::draw() {
	ofSetColor(0);
	stringstream text;
	text << "Connected to port " << midiOut.getPort()
		<< " \"" << midiOut.getName() << "\"" << endl
		<< "is virtual?: " << midiOut.isVirtual() << endl << endl
		<< "Sending to channel " << channel << endl << endl
		<< "MouseDrag" << endl
		<< "note: " << note << endl
		<< "velocity: " << velocity << endl
		<< "\n\nMidi Out Clock: " << (clockThread.isClockRunning() ? "on" : "off") << endl  // Usar isClockRunning
		<< "c: Start/Stop" << endl
		<< "BPM: " << clockThread.getBpm() << endl
		<< "Tempo +/-: " << clockThread.getBpm();
	ofDrawBitmapString(text.str(), 20, 20);

	if (ofGetElapsedTimeMillis() % 1000 == 0)
		ofLogNotice() << "\nMIDI Clock BPM: " << clockThread.getBpm();
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case 'c': // Start/Stop MIDI clock
		clockThread.toggleClock();  // Más simple usando toggleClock
		break;

	case '+': // Increase tempo
		clockThread.setBpm(clockThread.getBpm() + 1.0f);
		break;

	case '-': // Decrease tempo
		clockThread.setBpm(clockThread.getBpm() - 1.0f);
		break;
	}
}

void ofApp::mouseDragged(int x, int y, int button) {
	int note_ = ofMap(x, 0, ofGetWidth(), 0, 127);
	int velocity_ = ofRandom(32, 128);
	midiOut.sendNoteOn(channel, note_, velocity_);

	note = note_;
	velocity = velocity_;

	velocity_ = 0;
	midiOut << NoteOff(channel, note_, velocity_);
}
