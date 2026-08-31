#include "verovioscorewidget.h"
#include <QDebug>
#include <QRegularExpression>
#include <clocale>

#include "toolkit.h"
//#include "resources.h"

VerovioScoreWidget::VerovioScoreWidget(QWidget *parent)
    : QWebEngineView(parent)
{
    std::string resourcePath = "/home/kamil/Dokumenty/Projekty/QT/898SimpleMidiPiano/verovio-develop/data";
    //vrv::Resources::SetDefaultPath(resourcePath);

    m_toolkit = new vrv::Toolkit();
    m_toolkit->SetResourcePath(resourcePath);

    // Opcje konfiguracyjne Verovio
    std::string options = R"({
        "scale": 45,
        "pageWidth": 2000,
        "font": "Bravura",
        "adjustPageHeight": true
    })";

    m_toolkit->SetOptions(options);
}

VerovioScoreWidget::~VerovioScoreWidget()
{
    if (m_toolkit != nullptr) {
        delete m_toolkit;
        m_toolkit = nullptr;
    }
}

void VerovioScoreWidget::loadScoreData(const QString &data)
{
    if (data.isEmpty()) {
        qDebug() << "[Verovio] Puste dane lub czyszczenie ekranu.";
        this->setHtml("");
        m_currentSvgData.clear();
        return;
    }

    if (!m_toolkit->LoadData(data.toStdString())) {
        qDebug() << "[Verovio] Błąd parsowania danych.";
        return;
    }

    if (m_toolkit->GetPageCount() > 0) {
        std::string svg = m_toolkit->RenderToSVG(1);
        m_currentSvgData = QString::fromUtf8(svg.c_str());

        // Poprawka dla separatora dziesiętnego (scale)
        m_currentSvgData.replace(QRegularExpression("scale\\((\\d+),(\\d+),\\s*(\\d+),(\\d+)\\)"), "scale(\\1.\\2, \\3.\\4)");

        // Renderujemy HTML używając naszej bazowej grafiki
        updateHtmlView(m_currentSvgData);
    } else {
        this->setHtml("");
        m_currentSvgData.clear();
    }
}

// ---------------------------------------------------------------------
// FUNKCJE PODŚWIETLANIA I MANIPULACJI SVG
// ---------------------------------------------------------------------

QStringList VerovioScoreWidget::extractAllNoteIds()
{
    QStringList ids;
    if (m_currentSvgData.isEmpty()) return ids;

    // Uniwersalne wyrażenie: szuka każdego tagu <g>, niezależnie od kolejności atrybutów
    QRegularExpression re("<g\\s+([^>]+)>");
    QRegularExpressionMatchIterator i = re.globalMatch(m_currentSvgData);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString tagContent = match.captured(1);

        // Sprawdzamy czy ten element <g> jest nutą
        if (tagContent.contains("class=\"") && tagContent.contains("note")) {
            // Wyciągamy z niego identyfikator id="..."
            QRegularExpression idRe("id=\"([^\"]+)\"");
            QRegularExpressionMatch idMatch = idRe.match(tagContent);
            if (idMatch.hasMatch()) {
                ids.append(idMatch.captured(1));
            }
        }
    }

    qDebug() << "[Verovio] Sukces! Znaleziono ID nut do podświetlenia:" << ids.size();
    return ids;
}

void VerovioScoreWidget::highlightElements(const QStringList& elementIds, QString color)
{
    if (elementIds.isEmpty()) return;

    QString jsArray = "['" + elementIds.join("','") + "']";

    QString js = QString(R"(
        (function() {
            // 1. Resetujemy poprzednio podświetlone elementy (usuwamy fill i stroke)
            var oldEls = document.querySelectorAll('.highlighted-note');
            oldEls.forEach(function(el) {
                el.classList.remove('highlighted-note');
                el.style.fill = '';
                el.style.stroke = '';
                var paths = el.querySelectorAll('*');
                paths.forEach(function(p) { p.style.fill = ''; p.style.stroke = ''; });
            });

            // 2. Wyszukujemy nowe elementy i aplikujemy kolor na fill oraz stroke
            var ids = %1;
            ids.forEach(function(id) {
                var el = document.getElementById(id);
                if (el) {
                    el.classList.add('highlighted-note');
                    el.style.fill = '%2';
                    el.style.stroke = '%2';

                    var paths = el.querySelectorAll('*');
                    paths.forEach(function(p) {
                        p.style.fill = '%2';
                        p.style.stroke = '%2';
                    });
                }
            });
        })();
    )").arg(jsArray, color);

    this->page()->runJavaScript(js);
}
// ---------------------------------------------------------------------
// POMOCNICZY SZABLON HTML
// ---------------------------------------------------------------------

void VerovioScoreWidget::updateHtmlView(const QString &svgContent)
{
    QString html = QStringLiteral(R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        * {
            box-sizing: border-box;
        }
        html, body {
            margin: 0;
            padding: 10px;
            width: 100%;
            height: 100%;
            background-color: #2b2b2b; /* Ciemnoszare tło przestrzeni roboczej */
            display: flex;
            justify-content: center;
            align-items: flex-start;
            overflow-x: hidden;
        }
        svg {
            display: block;
            width: 100% !important;
            height: auto !important;
            background-color: #ffffff; /* Biała "kartka" papieru z nutami */
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.5);
            border-radius: 4px;
        }
    </style>
</head>
<body>
    %1
</body>
</html>
)").arg(svgContent);

    this->setHtml(html);
}
