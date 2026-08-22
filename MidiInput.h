#ifndef MIDIINPUT_H
#define MIDIINPUT_H

#include <QObject>
#include "RtMidi.h"

class MidiInput : public QObject {
    Q_OBJECT
public:
    explicit MidiInput(QObject *parent = nullptr);
    ~MidiInput();

signals:
    void noteOn(int note, int velocity);
    void noteOff(int note);

private:
    RtMidiIn *midiIn = nullptr;
    static void midiCallback(double deltatime, std::vector<unsigned char> *message, void *userData);
};

#endif // MIDIINPUT_H
