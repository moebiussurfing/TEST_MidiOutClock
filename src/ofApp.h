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
    }

    void play() {
        if (isThreadRunning()) return;

        {
            std::lock_guard<std::mutex> lock(mutex);
            isPlaying = true;
            songPositionBeats = 0;
        }

        if (midiOut) {
            // Send MIDI song position (0)
            midiOut->sendMidiByte(MIDI_SONG_POS_POINTER);
            midiOut->sendMidiByte(0x00); // LSB
            midiOut->sendMidiByte(0x00); // MSB

            // Send MIDI Start message
            midiOut->sendMidiByte(MIDI_START);
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
    }

protected:
    void threadedFunction() override {
        using clock = std::chrono::steady_clock;
        using namespace std::chrono;

        auto lastPulseTime = clock::now();
        uint64_t pulseCount = 0;

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
                    }
                }

                lastPulseTime += microseconds(microsPerPulse);

                // Calculate drift and adjust timing
                auto drift = duration_cast<microseconds>(now - lastPulseTime).count();
                if (drift > 0) {
                    lastPulseTime += microseconds(drift);
                }
            }

            // Yield thread for a short time to prevent CPU overload
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

	ofxMidiOut midiOut;

	int channel;
	int note, velocity;

private:
	MidiClockThread clockThread;
};