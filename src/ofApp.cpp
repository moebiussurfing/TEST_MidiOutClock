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

void ofApp::exit() {
	clockThread.stop();
	midiOut.closePort();
}

void ofApp::draw() {
	ofSetColor(0);
	stringstream text;
	text << "CONNECTED TO PORT " << midiOut.getPort()<<endl
		<< " \"" << midiOut.getName() << "\"" << endl
		<< "is virtual?: " << midiOut.isVirtual() << endl << endl
		<< "Sending to channel " << channel << endl << endl
		<< "MOUSEPRESS Test" << endl
		<< "note: " << note << endl
		<< "velocity: " << velocity << endl
		<< "\n\nMIDI OUT CLOCK: " << (clockThread.isClockRunning() ? "on" : "off") << endl  
		<< "Space key: Start/Stop" << endl
		<< "Tempo +/-: " << clockThread.getBpm() << endl
		<< "Reset backspace" << endl
		<< "BPM: " << clockThread.getBpm();
	ofDrawBitmapString(text.str(), 20, 20);

	if (ofGetElapsedTimeMillis() % 2000 == 0)
		ofLogNotice() << "MIDI Clock BPM: " << clockThread.getBpm();
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case ' ': // Start/Stop MIDI clock
		clockThread.toggleClock();
		break;

	case '+': // Increase tempo
		clockThread.setBpm(clockThread.getBpm() + 1.0f);
		break;

	case '-': // Decrease tempo
		clockThread.setBpm(clockThread.getBpm() - 1.0f);
		break;

	case OF_KEY_BACKSPACE: // Reset
		clockThread.setBpm(120.0f);
		break;
	}
}

void ofApp::mousePressed(int x, int y, int button) {
	int note_ = ofMap(x, 0, ofGetWidth(), 0, 127);
	int velocity_ = ofRandom(32, 128);
	midiOut.sendNoteOn(channel, note_, velocity_);

	note = note_;
	velocity = velocity_;

	velocity_ = 0;
	midiOut << NoteOff(channel, note_, velocity_);
}
