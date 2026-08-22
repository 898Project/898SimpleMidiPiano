#ifndef VEROVIOSCOREWIDGET_H
#define VEROVIOSCOREWIDGET_H

#include <QWebEngineView>
#include <QStringList>

namespace vrv {
class Toolkit;
}

class VerovioScoreWidget : public QWebEngineView
{
    Q_OBJECT

public:
    explicit VerovioScoreWidget(QWidget *parent = nullptr);
    ~VerovioScoreWidget();

    void loadScoreData(const QString &data);

    // --- NOWE FUNKCJE DO PODŚWIETLANIA ---
    QStringList extractAllNoteIds();
    void highlightElements(const QStringList& elementIds, QString color = "#e74c3c");

private:
    vrv::Toolkit *m_toolkit;

    // --- NOWE ZMIENNE I METODY ---
    QString m_currentSvgData; // Przechowuje oryginalny kod SVG wygenerowany przez Toolkit
    void updateHtmlView(const QString &svgContent); // Wyodrębniona metoda do generowania HTML
};

#endif // VEROVIOSCOREWIDGET_H
