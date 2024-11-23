#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

class MidiClockThread : public ofThread {
public:
    MidiClockThread() : bpm(120.0f), isPlaying(false), midiOut(nullptr), songPositionBeats(0) {}

    void setup(ofxMidiOut* output) {
        midiOut = output;
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

        // Recalculate timing values
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
            // Send MIDI realtime messages in sequence
            midiOut->sendMidiByte(MIDI_START);

            // Send song position (0)
            std::vector<unsigned char> songPos = {
                MIDI_SONG_POS_POINTER,
                0x00,  // LSB
                0x00   // MSB
            };
            midiOut->sendMidiBytes(songPos);
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

            // Calculate time since last pulse
            auto elapsedMicros = duration_cast<microseconds>(now - lastPulseTime).count();

            if (elapsedMicros >= microsPerPulse) {
                if (midiOut) {
                    // Send MIDI Clock pulse
                    midiOut->sendMidiByte(MIDI_TIME_CLOCK);

                    // Update song position every quarter note (24 pulses)
                    pulseCount++;
                    if (pulseCount % 24 == 0) {
                        std::lock_guard<std::mutex> lock(mutex);
                        songPositionBeats++;

                        // Optional: Send MIDI Song Position
                        uint16_t pos = songPositionBeats * 4; // Convert to MIDI beats (16th notes)
                        std::vector<unsigned char> songPos = {
                            MIDI_SONG_POS_POINTER,
                            static_cast<unsigned char>(pos & 0x7F),        // LSB
                            static_cast<unsigned char>((pos >> 7) & 0x7F)  // MSB
                        };
                        midiOut->sendMidiBytes(songPos);
                    }
                }

                // Reset for next pulse
                lastPulseTime = now;

                // High precision timing correction
                auto drift = elapsedMicros - microsPerPulse;
                if (drift > 0) {
                    lastPulseTime -= microseconds(drift);
                }
            }

            // Small sleep to prevent CPU overload while maintaining precision
            std::this_thread::sleep_for(microseconds(100));
        }
    }

private:
    float bpm;
    bool isPlaying;
    ofxMidiOut* midiOut;
    uint32_t songPositionBeats;  // Position in quarter notes
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