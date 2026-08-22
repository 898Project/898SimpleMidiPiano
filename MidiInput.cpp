
#include "MidiInput.h"
#include <QDebug>

MidiInput::MidiInput(QObject *parent) : QObject(parent) {
    try {
        midiIn = new RtMidiIn();
    } catch (RtMidiError &error) {
        qDebug() << "Błądt RtMidi:" << QString::fromStdString(error.getMessage());
        return;
    }

    unsigned int ports = midiIn->getPortCount();
    if (ports == 0) {
        qDebug() << "Brak dostępnych urządzeń MIDI!";
        return;
    }

    // Wypiszmy wszystkie porty, żebyś widział je w konsoli na każdym systemie
    for (unsigned int i = 0; i < ports; i++) {
        std::string portName = midiIn->getPortName(i);
        qDebug() << "Dostępny port MIDI [" << i << "]:" << QString::fromStdString(portName);
    }

    // Otwieramy domyślnie pierwszy port (indeks 0).
    // Na Windowsie zazwyczaj Twoja klawiatura od razu ląduje na pozycji 0 lub 1.
    unsigned int targetPort = 0; // Domyślnie 0

    // Automatyczny wybór: jeśli znajdziemy CH345, wybierzmy go!
    for (unsigned int i = 0; i < ports; i++) {
        std::string name = midiIn->getPortName(i);
        if (name.find("CH345") != std::string::npos) {
            targetPort = i;
            break;
        }
    }

    midiIn->openPort(targetPort);
    qDebug() << "Otwarto port MIDI o indeksie:" << targetPort;

    midiIn->setCallback(&MidiInput::midiCallback, this);
    midiIn->ignoreTypes(false, true, true);
}

MidiInput::~MidiInput() {
    if (midiIn) {
        delete midiIn;
    }
}

void MidiInput::midiCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    Q_UNUSED(deltatime);
    if (!message || message->empty()) return;

    MidiInput *self = static_cast<MidiInput*>(userData);

    int status = (*message)[0] & 0xF0;
    int key = (*message)[1];
    int velocity = (*message)[2];

    if (status == 0x90 && velocity > 0) {
        emit self->noteOn(key, velocity);
    } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
        emit self->noteOff(key);
    }
}
