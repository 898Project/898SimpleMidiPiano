#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <map>
#include <QString>
#include <QResizeEvent>
#include <QList>
#include <QTimer>
#if !defined(_WIN32)
#include <fluidsynth.h>
#endif
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "MidiInput.h"
#include "verovioscorewidget.h"
#include <QXmlStreamReader>
#include <QFile>
#include <QStandardPaths>

// Struktura przechowująca pojedynczy krok utworu (nutę lub cały akord)
struct PlaybackStep {
    QList<int> expectedNotes; // Nowe uderzenia w tym kroku (do zagrania)
    QList<int> tiedNotes;     // Nuty pod łukiem (do przytrzymania, bez ponownego uderzenia)
    QStringList svgElementIds;
    int delayToNext = 500;
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Zaktualizowany typ wyliczeniowy (dodano RandomMode)
enum class PracticeMode { WaitMode, PlayMode, RandomMode };

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
            void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleNoteOn(int note, int velocity);
    void handleNoteOff(int note);
    void otworzMidi();

    // --- Nowe sloty do obsługi trybów nauki ---
    void onModeChanged(int index);
    void generateNextRandomTask();
    void evaluateInput();
    void autoPlayNextStep();

private:
    // --- Struktury do przechowywania piosenki ---
    QList<PlaybackStep> songSequence; // Cały wczytany utwór
    int currentSequenceIndex = 0;     // Który krok utworu obecnie gramy/sprawdzamy
    QList<int> activeAutoPlayNotes;   // Nuty obecnie grające w autoodtwarzaniu (do wyłączenia)

    // Nowa metoda do parsowania XML
    void parseMusicXML(const QByteArray &xmlData);
    void startCurrentMode();
    Ui::MainWindow *ui;
    MidiInput *midiInput;
    std::map<int, int> activeNotes;

    QLabel *statusLabel;
    QLabel *scoreLabel;
    QComboBox *comboMode;
    VerovioScoreWidget *scoreWidget;

    // --- Zmienne metronomu ---
    QTimer *metronomeTimer;
    bool metronomeActive;
    int currentBpm;
    int currentBeatInMeasure;
    int timeBeats;

    // --- Zmienne do logiki gry i oceny ---
    PracticeMode currentMode;
    int correctNotesCount;
    int wrongNotesCount;
    int totalNotesInSong;

    QList<int> expectedNotes;         // Nuty, na które w danym momencie czeka program
    QList<int> currentlyPressedNotes; // Nuty aktualnie trzymane przez ucznia na klawiaturze
    QTimer *playbackTimer;            // Timer do Trybu Odtwarzania (Play Mode)

    // --- Zmienne FluidSynth ---
#if !defined(_WIN32)

    // tutaj inne pola/metody powiązane z FluidSynth

    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_audio_driver_t *adriver;
    #endif
    int soundFontId;

    bool isPlayingMidi;

    // --- Nowe metody pomocnicze ---
    void updateScoreDisplay();
    void resetScore();
};

#endif // MAINWINDOW_H
