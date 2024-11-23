#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

class MidiClockThread : public ofThread {
public:
	MidiClockThread() : bpm(120.0f), isPlaying(false), midiOut(nullptr), songPositionBeats(0) {}

	void setup(ofxMidiOut* output) {
		midiOut = output;
		setBpm(120.0f); // Initialize default tempo
	}

	void setBpm(float newBpm) {
		std::lock_guard<std::mutex> lock(mutex);
		bpm = std::clamp(newBpm, 20.0f, 300.0f);
		pulsesPerQuarterNote = 24; // MIDI standard

		// Use double for higher precision in calculation
		double microsecondsPerMinute = 60.0 * 1000000.0;
		double pulsesPerMinute = bpm * pulsesPerQuarterNote;
		microsPerPulse = static_cast<uint64_t>(microsecondsPerMinute / pulsesPerMinute);

		ofLogNotice() << "Clock settings:"
			<< " BPM: " << bpm
			<< " PPQN: " << pulsesPerQuarterNote
			<< " Micros per pulse: " << microsPerPulse;
	}

	float getBpm() {
		std::lock_guard<std::mutex> lock(mutex);
		return bpm;
	}

	void play() {
		if (isThreadRunning()) return;
		{
			std::lock_guard<std::mutex> lock(mutex);
			isPlaying = true;
			songPositionBeats = 0;
		}

		if (midiOut) {
			// Send song position (0)
			midiOut->sendMidiByte(MIDI_SONG_POS_POINTER);
			midiOut->sendMidiByte(0x00); // LSB
			midiOut->sendMidiByte(0x00); // MSB

			// Start playback
			midiOut->sendMidiByte(MIDI_START);

			ofLogNotice() << "MIDI Clock started";
		}

		startThread();
	}

	void stop() {
		if (!isThreadRunning()) return;

		if (midiOut) {
			midiOut->sendMidiByte(MIDI_STOP);
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			isPlaying = false;
		}

		waitForThread(true);
		ofLogNotice() << "MIDI Clock stopped";
	}

	void toggleClock() {
		if (isClockRunning()) {
			stop();
		}
		else {
			play();
		}
	}

	bool isClockRunning() const {
		return isPlaying && isThreadRunning();
	}

protected:
	void threadedFunction() override {
		using clock = std::chrono::steady_clock;
		using namespace std::chrono;

		const microseconds minSleepTime(100); // Minimum practical sleep time
		auto nextPulseTime = clock::now();
		uint64_t pulseCount = 0;

		while (isThreadRunning() && isPlaying) {
			auto now = clock::now();

			if (now >= nextPulseTime) {
				if (midiOut) {
					// Send MIDI Clock pulse precisely and atomically 
					std::lock_guard<std::mutex> lock(mutex);
					midiOut->sendMidiByte(MIDI_TIME_CLOCK);

					pulseCount++;
					if (pulseCount % pulsesPerQuarterNote == 0) {
						songPositionBeats++;
					}
				}

				// Calculate next pulse time
				nextPulseTime += microseconds(microsPerPulse);

				// Reset schedule if significantly behind
				if (now > nextPulseTime + microseconds(microsPerPulse * 4)) {
					ofLogWarning() << "MIDI Clock drift detected, resetting schedule";
					nextPulseTime = now + microseconds(microsPerPulse);
				}
			}

			// More precise sleep timing using spin-wait for fine-grained timing
			auto timeToNext = duration_cast<microseconds>(nextPulseTime - clock::now());
			if (timeToNext > minSleepTime) {
				std::this_thread::sleep_for(timeToNext - minSleepTime);
			}

			// Spin-wait for the remaining time
			while (clock::now() < nextPulseTime) {
				std::this_thread::yield();
			}

		}
	}

private:
	float bpm;
	bool isPlaying;
	ofxMidiOut* midiOut;
	uint32_t songPositionBeats;
	uint32_t pulsesPerQuarterNote;
	uint64_t microsPerPulse;
};

//--

class ofApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void exit();

	void keyPressed(int key);
	void mousePressed(int x, int y, int button);
	void mouseDragged(int x, int y, int button);

	ofxMidiOut midiOut;

	int channel;
	int note, velocity;

private:
	MidiClockThread clockThread;
};

//----

/*

Test with a MIDI monitor (like MIDI Monitor on macOS or Protokol) to verify the messages:


You should see F8 messages being sent regularly (24 per quarter note)
When starting: F2 00 00 followed by FA
When stopping: FC

In Ableton Live:

Ensure "Sync" is enabled in the MIDI preferences
Set the correct MIDI port as "Input"
Enable "Track" and "Sync" for that input port

This implementation should now work correctly with Ableton Live and other DAWs that accept MIDI clock sync.

*/

/*

Consider using high-resolution timer features on your specific OS for better precision:


Windows: timeBeginPeriod(1)
Linux: SCHED_FIFO scheduling
macOS: mach_absolute_time()

These changes should provide more stable timing and help reduce drift between master/slave applications. The statistics tracking will also help you monitor the actual vs target BPM.

*/

/*

Additional recommendations:

Consider buffering MIDI messages and sending them in batches to reduce system calls

*/