#include "mainwindowimpl.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QPrinter>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSvgRenderer>
#include <QtMath>

#include "definitionen.h"
#include "errormessage.h"
#include "mustache.h"

#include "svgview.h"

void MainWindowImpl::ErstelleFlaschenlabel() {
  QString current = comboBox_FLabelAuswahl->currentText();
  comboBox_FLabelAuswahl->clear();
  for (int i = 0; i < list_Anhang.count(); i++) {
    // wenn SVG dann auch der Flaschenlabel auswahl hinzufügen
    if (list_Anhang[i]->getPfad().indexOf(".svg") > 0) {
      comboBox_FLabelAuswahl->addItem(list_Anhang[i]->getPfad());
    }
  }
  comboBox_FLabelAuswahl->addItem("Beispiel Label");
  comboBox_FLabelAuswahl->setCurrentText(current);
}

void MainWindowImpl::LadeFlaschenlabel() {
  QString FullPfad;
  QString auswahl = comboBox_FLabelAuswahl->currentText();
  if (auswahl == "Beispiel Label") {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       KONFIG_ORDNER, APP_KONFIG);
    QString settingsPath = QFileInfo(settings.fileName()).absolutePath() + "/";
    FullPfad = settingsPath + "streifen_vorlage.svg";
  } else {
    for (int i = 0; i < list_Anhang.count(); i++) {
      if (list_Anhang[i]->getPfad() == comboBox_FLabelAuswahl->currentText()) {
        FullPfad = list_Anhang[i]->getFullPfad();
      }
    }
  }

  QSettings settings(QSettings::IniFormat, QSettings::UserScope, KONFIG_ORDNER,
                     APP_KONFIG);
  QString settingsPath = QFileInfo(settings.fileName()).absolutePath() + "/";

  QFile file(FullPfad);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  QString svg_template = file.readAll();

  QString svg = ErsetzeFlaschenlabeTags(svg_template);
  file.close();

  QFile filenew(settingsPath + "streifen.svg");
  if (filenew.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&filenew);
    out.setCodec("UTF-8");
    out << svg << endl;
    filenew.close();
  }

  FLabel_svgView->openFile(settingsPath + "streifen.svg");

  if (checkBox_FLabelSVGViewKontur->isChecked()) {
    FLabel_svgView->setViewOutline(true);
  } else {
    FLabel_svgView->setViewOutline(false);
  }
}

QString MainWindowImpl::ErsetzeFlaschenlabeTags(QString value) {
  QVariantHash contextVariables;
  ErstelleTagListe(contextVariables, true);

  Mustache::Renderer renderer;
  Mustache::QtVariantContext context(contextVariables);
  return renderer.render(value, &context);
}

void MainWindowImpl::on_pushButton_FlaschenlabelPDF_clicked() {
  QString Sudname = lineEdit_Sudname->text() + "_label";

  // letzten Pfad einlesen
  QSettings settings(QSettings::IniFormat, QSettings::UserScope, KONFIG_ORDNER,
                     APP_KONFIG);
  settings.beginGroup("PDF");
  QString p = settings.value("recentPDFPath", QDir::homePath()).toString();

  QString fileName =
      QFileDialog::getSaveFileName(this, trUtf8("PDF speichern unter"),
                                   p + "/" + Sudname + ".pdf", "PDF (*.pdf)");
  if (!fileName.isEmpty()) {
    // pdf speichern
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPrinter::A4);
    printer.setOrientation(QPrinter::Portrait);
    printer.setColorMode(QPrinter::Color);
    printer.setPageMargins(
        spinBox_FLabel_RandLinks->value(), spinBox_FLabel_RandOben->value(),
        spinBox_FLabel_RandRechts->value(), spinBox_FLabel_RandUnten->value(),
        QPrinter::Millimeter);
    printer.setFullPage(false);
    printer.setOutputFileName(fileName);
    printer.setOutputFormat(QPrinter::PdfFormat);
    //    printer.setOutputFormat(QPrinter::NativeFormat);

    const QSize sizeSVG = FLabel_svgView->svgSize();

    qreal seitenverhaeltnisSVG =
        qreal(sizeSVG.width()) / qreal(sizeSVG.height());
    qreal widthPageMM = printer.widthMM();
    qreal heightPageMM = printer.heightMM();
    qreal widthPagePx = printer.width();
    qreal faktorPxPerMM = widthPagePx / widthPageMM;

    qreal breiteMM = spinBox_BreiteLabel->value();
    qreal SVGhoeheMM = (breiteMM / seitenverhaeltnisSVG);
    qreal hoehePx = SVGhoeheMM * faktorPxPerMM;
    qreal abstandMM = spinBox_AbstandLabel->value();
    qreal abstandPx = abstandMM * faktorPxPerMM;

    // Gewünschte Anzahl
    int totalCount = spinBox_AnzahlLabels->value();

    // Anzahl der Streifen pro Seite
    int countPerPage = int(heightPageMM / (SVGhoeheMM + abstandMM));

    QPainter painter(&printer);
    int zaehler = 0;
    // Anzahl Seiten
    int pageCount =
        int(round(double(totalCount) / double(countPerPage) + double(0.5)));

    for (int seite = 0; seite < pageCount; seite++) {
      for (int i = 0; i < countPerPage; i++) {
        if (zaehler >= totalCount) {
          i = countPerPage;
        } else {
          FLabel_svgView->renderer()->render(
              &painter, QRectF(0, hoehePx * i + abstandPx * i,
                               breiteMM * faktorPxPerMM, hoehePx));

          if (checkBox_FLabelSVGViewKontur->isChecked()) {
            QRectF rect = QRectF(0, hoehePx * i + abstandPx * i,
                                 breiteMM * faktorPxPerMM, hoehePx);
            QPen outline(Qt::black, 10, Qt::DashLine);
            outline.setCosmetic(true);
            outline.setColor(QColor(255, 0, 0));
            painter.setPen(outline);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect);
          }

          zaehler++;
        }
      }
      if (zaehler < totalCount) {
        printer.newPage();
      }
    }
    painter.end();

    // Pfad abspeichern
    QFileInfo fi(fileName);
    settings.setValue("recentPDFPath", fi.absolutePath());

    // open PDF
    QDesktopServices::openUrl(QUrl("file:///" + fileName));
  }

  settings.endGroup();
}

void MainWindowImpl::on_checkBox_FLabelSVGViewKontur_stateChanged(int arg1) {
  FLabel_svgView->setViewOutline(arg1);
}

void MainWindowImpl::on_comboBox_FLabelAuswahl_activated(const QString &arg1) {
  if (arg1.isEmpty()) {
    FLabel_svgView->scene()->clear();
    checkBox_FLabelSVGViewKontur->setDisabled(true);
  } else {
    LadeFlaschenlabel();
    checkBox_FLabelSVGViewKontur->setDisabled(false);
  }
  setAenderung(true);
}

void MainWindowImpl::on_pushButton_FLabelTagNeu_clicked() {
  NewFLabelTag = true;
  int i = tableWidget_FLabelTags->rowCount();
  tableWidget_FLabelTags->setRowCount(i + 1);
  QTableWidgetItem *newItem1 = new QTableWidgetItem("");
  QTableWidgetItem *newItem2 = new QTableWidgetItem("");
  QTableWidgetItem *newItem3 = new QTableWidgetItem("");
  // Beschreibung
  newItem1->setText("Tagname");
  newItem2->setText("Value");
  newItem3->setCheckState(Qt::Unchecked);
  tableWidget_FLabelTags->setItem(i, 0, newItem1);
  tableWidget_FLabelTags->setItem(i, 1, newItem2);
  tableWidget_FLabelTags->setItem(i, 2, newItem3);
  NewFLabelTag = false;
  setAenderung(true);
}

void MainWindowImpl::on_pushButton_FLabelTagDel_clicked() {
  tableWidget_FLabelTags->removeRow(tableWidget_FLabelTags->currentRow());
  LadeFlaschenlabel();
  setAenderung(true);
}

void MainWindowImpl::on_tableWidget_FLabelTags_itemChanged(
    QTableWidgetItem *item) {
  if (!fuelleFlaschenlabelTags) {
    if (!NewFLabelTag) {
      LadeFlaschenlabel();
      setAenderung(true);
    }
  }
}
