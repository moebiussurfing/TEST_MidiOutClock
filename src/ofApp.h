#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

class MidiClockThread : public ofThread {
public:
    MidiClockThread() : bpm(120.0f), isPlaying(false), midiOut(nullptr) {}

    void setup(ofxMidiOut* output) {
        midiOut = output;
    }

    void setBpm(float newBpm) {
        std::lock_guard<std::mutex> lock(mutex);
        bpm = std::clamp(newBpm, 20.0f, 300.0f);
    }

    float getBpm() {
        std::lock_guard<std::mutex> lock(mutex);
        return bpm;
    }

    void play() {
        if (isThreadRunning()) return;
        isPlaying = true;
        startThread();
        if (midiOut) midiOut->sendMidiByte(MIDI_START);
    }

    void stop() {
        if (!isThreadRunning()) return;
        if (midiOut) midiOut->sendMidiByte(MIDI_STOP);
        isPlaying = false;
        waitForThread(true);
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
        using clock = std::chrono::steady_clock;  // steady_clock is better for precise timing
        using namespace std::chrono;

        auto start_time = clock::now();
        uint64_t pulse_count = 0;

        while (isThreadRunning() && isPlaying) {
            float current_bpm;
            {
                std::lock_guard<std::mutex> lock(mutex);
                current_bpm = bpm;
            }

            // Precise calculation of MIDI pulse interval (24 ppqn)
            double microsPerPulse = (60.0 * 1000000.0) / (current_bpm * 24.0);

            // Calculate exact next pulse time based on start_time
            auto next_pulse_time = start_time + microseconds(static_cast<uint64_t>(microsPerPulse * pulse_count));
            auto now = clock::now();

            if (now >= next_pulse_time) {
                // Send MIDI clock
                if (midiOut) midiOut->sendMidiByte(MIDI_TIME_CLOCK);
                pulse_count++;

                // Resync if we've fallen significantly behind
                auto drift = duration_cast<microseconds>(now - next_pulse_time).count();
                if (drift > microsPerPulse * 2) {
                    start_time = now;
                    pulse_count = 0;
                    ofLogNotice() << "MIDI Clock: Resync triggered. Drift: " << drift << " microseconds";
                }
            }
            else {
                // Sleep until just before next pulse
                auto sleep_duration = next_pulse_time - now;
                if (sleep_duration > microseconds(100)) {
                    sleep_duration = sleep_duration - microseconds(50); // Wake up slightly early for better precision
                    std::this_thread::sleep_for(sleep_duration);
                }
            }
        }
    }

private:
    float bpm;
    bool isPlaying;
    ofxMidiOut* midiOut;
};

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void exit();

    void keyPressed(int key);
    void mouseDragged(int x, int y, int button);

    ofxMidiOut midiOut;

    int channel;
    int note, velocity;

private:
    MidiClockThread clockThread;
};