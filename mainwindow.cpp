#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "verovioscorewidget.h"
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QFile>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
#if !defined(_WIN32)
    , settings(nullptr)
    , synth(nullptr)
    , adriver(nullptr)
#endif
    , soundFontId(-1)
    , isPlayingMidi(false)
    , currentBpm(120)
    , metronomeActive(false)
    , currentBeatInMeasure(0)
    , timeBeats(4)
    , currentMode(PracticeMode::WaitMode)
    , correctNotesCount(0)
    , wrongNotesCount(0)
    , totalNotesInSong(0)
{
    ui->setupUi(this);

#if !defined(_WIN32)
    // --- Inicjalizacja FluidSynth oraz SoundFonta z zasobów Qt ---
    settings = new_fluid_settings();
    fluid_settings_setint(settings, "synth.polyphony", 64);
    synth = new_fluid_synth(settings);
    adriver = new_fluid_audio_driver(settings, synth);

    QString sourcePath = ":/soundfont.sf2";
    QString tempPath = QDir::tempPath() + "/soundfont.sf2";

    if (!QFile::exists(sourcePath)) {
        qDebug() << "KRYTYCZNY BŁĄD: Nie mogę znaleźć pliku zasobu:" << sourcePath;
    } else {
        if (QFile::exists(tempPath)) QFile::remove(tempPath);
        if (QFile::copy(sourcePath, tempPath)) {
            soundFontId = fluid_synth_sfload(synth, tempPath.toStdString().c_str(), 1);
            if (soundFontId == FLUID_FAILED) {
                qDebug() << "BŁĄD: Nie udało się załadować SoundFontu do syntezatora!";
            } else {
                qDebug() << "Sukces: SoundFont załadowany pomyślnie!";
            }
        }
    }
#endif

    // --- Główny układ okna ---
    QVBoxLayout *mainLayout = new QVBoxLayout(ui->centralwidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // --- PANEL GÓRNY (informacje / status) ---
    QHBoxLayout *topInfoLayout = new QHBoxLayout();
    statusLabel = new QLabel("Wybierz tryb z listy poniżej", this);
    statusLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e74c3c; background: transparent;");
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    scoreLabel = new QLabel(this);
    scoreLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #2c3e50; padding: 4px;");
    scoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resetScore();

    topInfoLayout->addWidget(statusLabel);
    topInfoLayout->addWidget(scoreLabel);
    mainLayout->addLayout(topInfoLayout, 0);

    // --- WIDOK NUT (VEROVIO) ---
    scoreWidget = new VerovioScoreWidget(this);
    scoreWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(scoreWidget, 1);

    // --- PANEL STEROWANIA (DÓŁ) ---
    QHBoxLayout *controlLayout = new QHBoxLayout();

    QPushButton *btnOpen = new QPushButton("Wybierz plik muzyczny", this);
    controlLayout->addWidget(btnOpen);

    comboMode = new QComboBox(this);
    comboMode->addItem("Tryb Czekania (Z pliku)", static_cast<int>(PracticeMode::WaitMode));
    comboMode->addItem("Tryb Odtwarzania (Z pliku)", static_cast<int>(PracticeMode::PlayMode));
    comboMode->addItem("Tryb Losowych Nut (A Vista)", static_cast<int>(PracticeMode::RandomMode));
    controlLayout->addWidget(comboMode);

    QCheckBox *chkMetronome = new QCheckBox("Metronom", this);
    controlLayout->addWidget(chkMetronome);

    QLabel *lblTempo = new QLabel("Tempo (BPM):", this);
    controlLayout->addWidget(lblTempo);

    QSpinBox *spinTempo = new QSpinBox(this);
    spinTempo->setRange(40, 240);
    spinTempo->setValue(currentBpm);
    controlLayout->addWidget(spinTempo);

    mainLayout->addLayout(controlLayout, 0);

    // --- POŁĄCZENIA SYGNAŁÓW (UI) ---
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::otworzMidi);

    connect(comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);

    // --- Timery ---
    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::autoPlayNextStep);

    metronomeTimer = new QTimer(this);
    connect(metronomeTimer, &QTimer::timeout, this, [this]() {
#if !defined(_WIN32)
        if (!metronomeActive || !synth) return;
        int noteToPlay = (currentBeatInMeasure == 0) ? 76 : 77;
        fluid_synth_noteon(synth, 9, noteToPlay, 100);
#else
        if (!metronomeActive) return;
#endif
        currentBeatInMeasure = (currentBeatInMeasure + 1) % timeBeats;
    });

    connect(chkMetronome, &QCheckBox::toggled, this, [this](bool checked) {
        metronomeActive = checked;
        currentBeatInMeasure = 0;
        if (metronomeActive) metronomeTimer->start(60000 / currentBpm);
        else metronomeTimer->stop();
    });

    connect(spinTempo, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        currentBpm = value;
        if (metronomeActive) metronomeTimer->start(60000 / currentBpm);
    });

    // --- Inicjalizacja wejścia MIDI ---
    midiInput = new MidiInput(this);
    connect(midiInput, &MidiInput::noteOn, this, &MainWindow::handleNoteOn);
    connect(midiInput, &MidiInput::noteOff, this, &MainWindow::handleNoteOff);
}

MainWindow::~MainWindow()
{
    if (playbackTimer) playbackTimer->stop();
    if (metronomeTimer) metronomeTimer->stop();
#if !defined(_WIN32)
    if (adriver) delete_fluid_audio_driver(adriver);
    if (synth) delete_fluid_synth(synth);
    if (settings) delete_fluid_settings(settings);
#endif
    delete ui;
}

void MainWindow::onModeChanged(int index)
{
    currentMode = static_cast<PracticeMode>(index);
    resetScore();
    playbackTimer->stop();
    expectedNotes.clear();
    currentlyPressedNotes.clear();

    if (currentMode == PracticeMode::RandomMode) {
        statusLabel->setText("Zagraj nutę widoczną na ekranie.");
        generateNextRandomTask();
    } else if (currentMode == PracticeMode::WaitMode) {
        statusLabel->setText("Wczytaj plik, aby rozpocząć grę (Czekanie).");
        if(scoreWidget) scoreWidget->loadScoreData("");
    } else if (currentMode == PracticeMode::PlayMode) {
        statusLabel->setText("Wczytaj plik i kliknij Play, aby odtworzyć.");
        if(scoreWidget) scoreWidget->loadScoreData("");
    }
}

void MainWindow::handleNoteOn(int note, int velocity) {
#if !defined(_WIN32)
    if (synth) fluid_synth_noteon(synth, 0, note, velocity);
#endif

    if (currentMode == PracticeMode::WaitMode || currentMode == PracticeMode::RandomMode) {
        if (!currentlyPressedNotes.contains(note)) {
            currentlyPressedNotes.append(note);
        }
        evaluateInput();
    }
}

void MainWindow::handleNoteOff(int note) {
#if !defined(_WIN32)
    if (synth) fluid_synth_noteoff(synth, 0, note);
#endif

    currentlyPressedNotes.removeAll(note);
}

void MainWindow::evaluateInput()
{
    if (expectedNotes.isEmpty()) return;

    bool allCorrect = true;
    bool hasWrongNote = false;

    for (int pressedNote : currentlyPressedNotes) {
        if (!expectedNotes.contains(pressedNote)) {
            hasWrongNote = true;
            break;
        }
    }

    if (hasWrongNote) {
        wrongNotesCount++;
        updateScoreDisplay();
        statusLabel->setText("Pudło! Spróbuj ponownie.");
        return;
    }

    for (int expectedNote : expectedNotes) {
        if (!currentlyPressedNotes.contains(expectedNote)) {
            allCorrect = false;
            break;
        }
    }

    if (allCorrect) {
        correctNotesCount++;
        updateScoreDisplay();
        statusLabel->setText("Dobrze!");

        if (currentMode == PracticeMode::RandomMode) {
            generateNextRandomTask();
        } else if (currentMode == PracticeMode::WaitMode) {
            currentlyPressedNotes.clear();

            do {
                currentSequenceIndex++;
            } while (currentSequenceIndex < songSequence.size() &&
                     songSequence[currentSequenceIndex].expectedNotes.isEmpty());

            if (currentSequenceIndex < songSequence.size()) {
                expectedNotes = songSequence[currentSequenceIndex].expectedNotes;

                if (scoreWidget) {
                    scoreWidget->highlightElements(songSequence[currentSequenceIndex].svgElementIds, "#3498db");
                }
            } else {
                statusLabel->setText("Utwór Zakończony! Gratulacje!");
                expectedNotes.clear();
            }
        }
    }
}

void MainWindow::updateScoreDisplay()
{
    int total = correctNotesCount + wrongNotesCount;
    int accuracy = (total == 0) ? 100 : (correctNotesCount * 100) / total;

    QString scoreText = QString("Poprawne: %1 | Błędne: %2 | Celność: %3%")
                            .arg(correctNotesCount)
                            .arg(wrongNotesCount)
                            .arg(accuracy);
    scoreLabel->setText(scoreText);
}

void MainWindow::resetScore()
{
    correctNotesCount = 0;
    wrongNotesCount = 0;
    updateScoreDisplay();
}

void MainWindow::generateNextRandomTask()
{
    int randomMidiPitch = QRandomGenerator::global()->bounded(60, 73);

    expectedNotes.clear();
    expectedNotes.append(randomMidiPitch);

    const char* stepNames[] = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};
    QString step = stepNames[randomMidiPitch % 12];
    int octave = (randomMidiPitch / 12) - 1;

    bool isSharp = (randomMidiPitch % 12 == 1 || randomMidiPitch % 12 == 3 ||
                    randomMidiPitch % 12 == 6 || randomMidiPitch % 12 == 8 ||
                    randomMidiPitch % 12 == 10);

    QString alterTag = isSharp ? "<alter>1</alter>" : "";

    QString xmlData = QString(
                          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
                          "<score-partwise version=\"3.1\">\n"
                          "  <part-list><score-part id=\"P1\"><part-name>Piano</part-name></score-part></part-list>\n"
                          "  <part id=\"P1\">\n"
                          "    <measure number=\"1\">\n"
                          "      <attributes>\n"
                          "        <divisions>1</divisions>\n"
                          "        <key><fifths>0</fifths></key>\n"
                          "        <time><beats>4</beats><beat-type>4</beat-type></time>\n"
                          "        <clef><sign>G</sign><line>2</line></clef>\n"
                          "      </attributes>\n"
                          "      <note>\n"
                          "        <pitch>\n"
                          "          <step>%1</step>\n"
                          "          %2\n"
                          "          <octave>%3</octave>\n"
                          "        </pitch>\n"
                          "        <duration>4</duration>\n"
                          "        <type>whole</type>\n"
                          "      </note>\n"
                          "    </measure>\n"
                          "  </part>\n"
                          "</score-partwise>"
                          ).arg(step).arg(alterTag).arg(octave);

    if(scoreWidget) {
        scoreWidget->loadScoreData(xmlData);
    }
}

void MainWindow::otworzMidi() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("Otwórz plik muzyczny"), "",
                                                    tr("Pliki MusicXML (*.xml *.musicxml);;Wszystkie pliki (*.*)"));

    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QFile::ReadOnly)) {
            QByteArray rawData = file.readAll();
            file.close();

            QString xmlStr = QString::fromUtf8(rawData);
            xmlStr.remove(QRegularExpression("<stem>.*?</stem>"));
            QByteArray cleanedData = xmlStr.toUtf8();

            if (scoreWidget) scoreWidget->loadScoreData(cleanedData);
            parseMusicXML(cleanedData);
            startCurrentMode();

        } else {
            QMessageBox::warning(this, "Błąd", "Nie udało się otworzyć wybranego pliku.");
        }
    }
}

void MainWindow::parseMusicXML(const QByteArray &xmlData) {
    songSequence.clear();
    QXmlStreamReader xml(xmlData);

    bool isChord = false;
    bool isRest = false;
    bool isTieStop = false;

    int currentBasePitch = -1;
    int currentAlter = 0;
    int currentOctave = 4;

    int currentDivisions = 1;
    int currentDuration = 1;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();

            if (name == "note") {
                isChord = false;
                isRest = false;
                isTieStop = false;
                currentBasePitch = -1;
                currentAlter = 0;
            } else if (name == "tie" || name == "tied") {
                if (xml.attributes().value("type").toString() == "stop") {
                    isTieStop = true;
                }
            } else if (name == "chord") {
                isChord = true;
            } else if (name == "rest") {
                isRest = true;
            } else if (name == "step") {
                QString step = xml.readElementText();
                if (step == "C") currentBasePitch = 0;
                else if (step == "D") currentBasePitch = 2;
                else if (step == "E") currentBasePitch = 4;
                else if (step == "F") currentBasePitch = 5;
                else if (step == "G") currentBasePitch = 7;
                else if (step == "A") currentBasePitch = 9;
                else if (step == "B") currentBasePitch = 11;
            } else if (name == "alter") {
                currentAlter = xml.readElementText().toInt();
            } else if (name == "octave") {
                currentOctave = xml.readElementText().toInt();
            } else if (name == "divisions") {
                currentDivisions = xml.readElementText().toInt();
                if (currentDivisions == 0) currentDivisions = 1;
            } else if (name == "duration") {
                currentDuration = xml.readElementText().toInt();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == QLatin1String("note")) {

                int delayMs = (60000.0 / currentBpm) * ((double)currentDuration / currentDivisions);

                if (!isRest && currentBasePitch != -1) {
                    int midiPitch = (currentOctave + 1) * 12 + currentBasePitch + currentAlter;

                    if (isChord && !songSequence.isEmpty()) {
                        if (isTieStop) {
                            songSequence.last().tiedNotes.append(midiPitch);
                        } else {
                            songSequence.last().expectedNotes.append(midiPitch);
                        }
                    } else {
                        PlaybackStep step;
                        step.delayToNext = delayMs;

                        if (isTieStop) step.tiedNotes.append(midiPitch);
                        else step.expectedNotes.append(midiPitch);

                        songSequence.append(step);
                    }
                } else if (isRest) {
                    PlaybackStep step;
                    step.delayToNext = delayMs;
                    songSequence.append(step);
                }
            }
        }
    }

    if (scoreWidget) {
        QStringList generatedIds = scoreWidget->extractAllNoteIds();
        int idIndex = 0;

        for (int i = 0; i < songSequence.size(); ++i) {
            for (int j = 0; j < songSequence[i].expectedNotes.size(); ++j) {
                if (idIndex < generatedIds.size()) songSequence[i].svgElementIds.append(generatedIds[idIndex++]);
            }
            for (int j = 0; j < songSequence[i].tiedNotes.size(); ++j) {
                if (idIndex < generatedIds.size()) songSequence[i].svgElementIds.append(generatedIds[idIndex++]);
            }
        }
    }
}

void MainWindow::autoPlayNextStep() {
    if (songSequence.isEmpty()) return;

    if (currentSequenceIndex >= songSequence.size()) {
#if !defined(_WIN32)
        for (int note : activeAutoPlayNotes) {
            if (synth) fluid_synth_noteoff(synth, 0, note);
        }
#endif
        activeAutoPlayNotes.clear();
        playbackTimer->stop();
        statusLabel->setText("Koniec odtwarzania.");
        return;
    }

    PlaybackStep step = songSequence[currentSequenceIndex];

    QList<int> continuingNotes = step.tiedNotes;
    QList<int> newExpectedNotes = step.expectedNotes;

    QList<int> notesToStop;
    for (int note : activeAutoPlayNotes) {
        if (!newExpectedNotes.contains(note) && !continuingNotes.contains(note)) {
            notesToStop.append(note);
        }
    }

#if !defined(_WIN32)
    for (int note : notesToStop) {
        if (synth) fluid_synth_noteoff(synth, 0, note);
    }
#endif

    QList<int> nextActiveNotes;

    for (int note : continuingNotes) {
        if (activeAutoPlayNotes.contains(note)) {
            nextActiveNotes.append(note);
        }
    }

    for (int note : newExpectedNotes) {
#if !defined(_WIN32)
        if (synth) fluid_synth_noteon(synth, 0, note, 100);
#endif
        if (!nextActiveNotes.contains(note)) {
            nextActiveNotes.append(note);
        }
    }

    activeAutoPlayNotes = nextActiveNotes;

    if (scoreWidget) {
        scoreWidget->highlightElements(step.svgElementIds, "#e74c3c");
    }

    currentSequenceIndex++;
    playbackTimer->start(step.delayToNext);
}

void MainWindow::startCurrentMode() {
    playbackTimer->stop();
    resetScore();
    currentlyPressedNotes.clear();
    currentSequenceIndex = 0;

    if (songSequence.isEmpty() && currentMode != PracticeMode::RandomMode) {
        statusLabel->setText("Proszę wczytać utwór.");
        return;
    }

    if (currentMode == PracticeMode::WaitMode) {
        while (currentSequenceIndex < songSequence.size() &&
               songSequence[currentSequenceIndex].expectedNotes.isEmpty()) {
            currentSequenceIndex++;
        }
        if (currentSequenceIndex >= songSequence.size()) {
            statusLabel->setText("Brak nut do zagrania w tym pliku.");
            expectedNotes.clear();
            return;
        }
        statusLabel->setText("Graj! Czekam na pierwszą nutę...");
        expectedNotes = songSequence[currentSequenceIndex].expectedNotes;

        if (scoreWidget) {
            scoreWidget->highlightElements(songSequence[currentSequenceIndex].svgElementIds, "#3498db");
        }
    } else if (currentMode == PracticeMode::PlayMode) {
        statusLabel->setText("Odtwarzanie...");
        playbackTimer->start(500);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    int dynamicSize = qMax(12, this->width() / 45);
    QString scoreStyle = QString("font-size: %1px; font-weight: bold; color: #2c3e50; padding: 4px;").arg(dynamicSize);
    scoreLabel->setStyleSheet(scoreStyle);

    int statusSize = qMax(12, this->width() / 50);
    QString statusStyle = QString("font-size: %1px; font-weight: bold; color: #e74c3c; background: transparent;").arg(statusSize);
    statusLabel->setStyleSheet(statusStyle);
}
