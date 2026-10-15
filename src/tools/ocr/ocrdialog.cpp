// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrdialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

OcrDialog::OcrDialog(const QPixmap& capture, QWidget* parent)
  : QDialog(parent)
  , m_inputFile(QDir::tempPath() + "/flameshot-ocr-XXXXXX.png")
  , m_timeout(this)
{
    setWindowTitle(tr("Recognize Text"));
    resize(640, 440);

    auto* layout = new QVBoxLayout(this);
    m_status =
      new QLabel(tr("Recognizing Chinese and English text locally…"), this);
    m_status->setObjectName("ocrStatus");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_text = new QPlainTextEdit(this);
    m_text->setObjectName("ocrText");
    m_text->setAccessibleName(tr("Recognized text"));
    m_text->setReadOnly(true);
    layout->addWidget(m_text);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_copy = buttons->addButton(tr("Copy Text"), QDialogButtonBox::AcceptRole);
    m_copy->setObjectName("ocrCopy");
    m_copy->setEnabled(false);
    m_copy->setAutoDefault(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_copy, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_text, &QPlainTextEdit::textChanged, this, [this]() {
        m_copy->setEnabled(!m_text->isReadOnly() &&
                           !m_text->toPlainText().trimmed().isEmpty());
    });

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(60000);
    m_timeout.setObjectName("ocrTimeout");
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        fail(tr("Text recognition timed out. Try a smaller selection."));
    });
    connect(&m_process, &QProcess::finished, this, &OcrDialog::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this]() {
        fail(tr("Could not run Tesseract: %1").arg(m_process.errorString()));
    });
    // Start after the dialog is visible, keeping the capture UI responsive.
    QTimer::singleShot(0, this, [this, capture]() { start(capture); });
}

OcrDialog::~OcrDialog()
{
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    // m_inputFile removes the screenshot after the child process has stopped.
}

QString OcrDialog::text() const
{
    return m_text->toPlainText();
}

void OcrDialog::start(const QPixmap& capture)
{
    // Local builds keep optional runtime dependencies next to the build's src
    // directory. Resolve them independently of the shell or desktop launcher.
    const QString prefix = QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath("../local-deps/usr");
    m_executable =
      QStandardPaths::findExecutable("tesseract", { prefix + "/bin" });
    if (!m_executable.isEmpty()) {
        auto environment = QProcessEnvironment::systemEnvironment();
        const QDir libraryDir(prefix + "/lib");
        QStringList libraryPaths;
        for (const QString& directory : libraryDir.entryList(
               { "*-linux-gnu" }, QDir::Dirs | QDir::NoDotAndDotDot)) {
            libraryPaths.append(libraryDir.filePath(directory));
        }
        libraryPaths.append(libraryDir.absolutePath());
        const QString existingPaths = environment.value("LD_LIBRARY_PATH");
        if (!existingPaths.isEmpty()) {
            libraryPaths.append(existingPaths);
        }
        environment.insert("LD_LIBRARY_PATH", libraryPaths.join(':'));

        const QDir dataRoot(prefix + "/share/tesseract-ocr");
        for (const QString& version :
             dataRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                QDir::Name | QDir::Reversed)) {
            const QString data = dataRoot.filePath(version + "/tessdata");
            if (QFileInfo::exists(data + "/eng.traineddata") &&
                QFileInfo::exists(data + "/chi_sim.traineddata")) {
                environment.insert("TESSDATA_PREFIX", data);
                break;
            }
        }
        m_process.setProcessEnvironment(environment);
    } else {
        m_executable = QStandardPaths::findExecutable("tesseract");
    }
    if (m_executable.isEmpty()) {
        fail(
          tr("Tesseract was not found. Install Tesseract OCR and its English "
             "(eng) and Simplified Chinese (chi_sim) language data, then "
             "make sure tesseract is on PATH."));
        return;
    }
    if (capture.isNull() || !m_inputFile.open() ||
        !capture.save(&m_inputFile, "PNG")) {
        fail(tr("Could not prepare the selected image for text recognition."));
        return;
    }
    m_inputFile.close();
    m_timeout.start();
    m_process.start(m_executable, { "--list-langs" });
}

void OcrDialog::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_done) {
        return;
    }
    const QString output = QString::fromUtf8(m_process.readAllStandardOutput());
    const QString errors = QString::fromUtf8(m_process.readAllStandardError());
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        fail(
          tr("Text recognition failed: %1").arg(errors.trimmed().left(2000)));
        return;
    }
    if (m_checkingLanguages) {
        QStringList languages = output.split('\n');
        for (auto& language : languages) {
            language = language.trimmed();
        }
        if (!languages.contains("eng") || !languages.contains("chi_sim")) {
            fail(
              tr("English (eng) and Simplified Chinese (chi_sim) OCR "
                 "language data are required. Install both language packs "
                 "and check TESSDATA_PREFIX if you use a local installation."));
            return;
        }
        m_checkingLanguages = false;
        m_process.start(m_executable,
                        { m_inputFile.fileName(),
                          "stdout",
                          "-l",
                          "chi_sim+eng",
                          "--psm",
                          "11" });
        return;
    }
    m_done = true;
    m_timeout.stop();
    if (output.trimmed().isEmpty()) {
        m_status->setText(tr("No text was found. Cancel and select a clearer "
                             "or larger text area."));
        return;
    }
    m_status->setText(
      tr("Recognition complete. You can edit the text before copying."));
    m_text->setReadOnly(false);
    m_text->setPlainText(output.trimmed());
    m_text->setFocus();
}

void OcrDialog::fail(const QString& message)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
    }
    m_status->setText(message);
    m_copy->setEnabled(false);
}
