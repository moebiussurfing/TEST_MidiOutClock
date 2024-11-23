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

    float getBpm() {
        std::lock_guard<std::mutex> lock(mutex);
        return bpm;
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

    void setBpm(float newBpm) {
        std::lock_guard<std::mutex> lock(mutex);
        bpm = std::clamp(newBpm, 20.0f, 300.0f);
        pulsesPerQuarterNote = 24; // MIDI standard
        microsPerPulse = static_cast<uint64_t>((60.0 * 1000000.0) / (bpm * pulsesPerQuarterNote));

        // Log timing details
        ofLogNotice() << "Clock settings:"
            << " BPM: " << bpm
            << " Micros per pulse: " << microsPerPulse;
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

protected:
    void threadedFunction() override {
        using clock = std::chrono::steady_clock;
        using namespace std::chrono;

        auto lastPulseTime = clock::now();
        uint64_t pulseCount = 0;
        bool logNextBeat = true;

        while (isThreadRunning() && isPlaying) {
            auto now = clock::now();
            auto elapsedMicros = duration_cast<microseconds>(now - lastPulseTime).count();

            if (elapsedMicros >= microsPerPulse) {
                if (midiOut) {
                    // Send MIDI Clock pulse (0xF8)
                    midiOut->sendMidiByte(MIDI_TIME_CLOCK);

                    pulseCount++;
                    if (pulseCount % pulsesPerQuarterNote == 0) {
                        std::lock_guard<std::mutex> lock(mutex);
                        songPositionBeats++;

                        // Log timing accuracy every bar (4 beats)
                        if (songPositionBeats % 4 == 0 && logNextBeat) {
                            auto actualPeriod = duration_cast<microseconds>(
                                now - lastPulseTime).count();
                            auto actualBpm = (60.0 * 1000000.0) /
                                (actualPeriod * pulsesPerQuarterNote);
                            ofLogNotice() << "MIDI Clock stats - Target BPM: "
                                << bpm << " Actual BPM: " << actualBpm;
                            logNextBeat = false;
                        }
                        else {
                            logNextBeat = true;
                        }
                    }
                }

                lastPulseTime += microseconds(microsPerPulse);

                // Timing adjustment
                auto drift = duration_cast<microseconds>(now - lastPulseTime).count();
                if (drift > 0) {
                    lastPulseTime += microseconds(drift);
                }
            }

            // Thread timing
            std::this_thread::sleep_for(microseconds(100));
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