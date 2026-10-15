// SPDX-License-Identifier: GPL-3.0-or-later

#include "tools/ocr/ocrdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>
#include <cerrno>
#include <csignal>

class OcrDialogTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_directory;
    QByteArray m_originalPath;

    QPixmap capture()
    {
        QPixmap image(200, 80);
        image.fill(Qt::white);
        return image;
    }

    QString status(OcrDialog& dialog)
    {
        return dialog.findChild<QLabel*>("ocrStatus")->text();
    }

    QByteArray recorded(const QString& name)
    {
        QFile file(m_directory.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll().trimmed();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_originalPath = qgetenv("PATH");
        QFile engine(m_directory.filePath("tesseract"));
        QVERIFY(
          QFile::copy(QStringLiteral(OCR_TEST_SOURCE_DIR "/fake-tesseract.sh"),
                      engine.fileName()));
        QVERIFY(engine.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner));
        qputenv("FLAMESHOT_OCR_TEST_DIR", m_directory.path().toUtf8());
    }

    void init()
    {
        qputenv("PATH", m_directory.path().toUtf8());
        qputenv("FLAMESHOT_OCR_TEST_MODE", "success");
        QFile::remove(m_directory.filePath("image"));
        QFile::remove(m_directory.filePath("pid"));
        QApplication::clipboard()->setText("original clipboard");
    }

    void cleanupTestCase()
    {
        qputenv("PATH", m_originalPath);
        qunsetenv("FLAMESHOT_OCR_TEST_DIR");
        qunsetenv("FLAMESHOT_OCR_TEST_MODE");
    }

    void acceptsEditedUnicodeAndRemovesImage()
    {
        QString imagePath;
        {
            OcrDialog dialog(capture());
            dialog.show();
            auto* text = dialog.findChild<QPlainTextEdit*>("ocrText");
            auto* copy = dialog.findChild<QPushButton*>("ocrCopy");
            QVERIFY(!copy->isEnabled());
            QTRY_COMPARE(text->toPlainText(),
                         QString::fromUtf8("中文识别\nHello OCR 123"));
            imagePath = QString::fromUtf8(recorded("image"));
            QVERIFY(QFile::exists(imagePath));
            QVERIFY(!QPixmap(imagePath).isNull());
            text->clear();
            QVERIFY(!copy->isEnabled());
            text->setPlainText(QString::fromUtf8("修正后的文字\nEdited text"));
            QVERIFY(copy->isEnabled());
            copy->click();
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QCOMPARE(dialog.text(), text->toPlainText());
        }
        QVERIFY(!QFile::exists(imagePath));
    }

    void reportsMissingEngine()
    {
        qputenv("PATH", m_directory.filePath("missing").toUtf8());
        OcrDialog dialog(capture());
        QTRY_VERIFY(status(dialog).contains("Tesseract was not found"));
        QVERIFY(!dialog.findChild<QPushButton*>("ocrCopy")->isEnabled());
        QCOMPARE(QApplication::clipboard()->text(),
                 QString("original clipboard"));
    }

    void localInstallationWorksWithoutPath()
    {
        const QString root = m_directory.filePath("bundle");
        const QString prefix = root + "/local-deps/usr";
        QVERIFY(QDir().mkpath(root + "/src"));
        QVERIFY(QDir().mkpath(prefix + "/bin"));
        QVERIFY(QDir().mkpath(prefix + "/lib/x86_64-linux-gnu"));
        const QString data = prefix + "/share/tesseract-ocr/4.00/tessdata";
        QVERIFY(QDir().mkpath(data));
        for (const QString& language : { QString("eng"), QString("chi_sim") }) {
            QFile file(data + "/" + language + ".traineddata");
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(QFile::copy(m_directory.filePath("tesseract"),
                            prefix + "/bin/tesseract"));
        const QString program = root + "/src/ocr_dialog_test";
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), program));
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("FLAMESHOT_OCR_BUNDLE_TEST", "1");
        child.setProcessEnvironment(environment);
        child.start(program, { "recognizesFromLocalInstallation" });
        QVERIFY(child.waitForFinished(10000));
        const QByteArray output =
          child.readAllStandardOutput() + child.readAllStandardError();
        QVERIFY2(child.exitStatus() == QProcess::NormalExit &&
                   child.exitCode() == 0,
                 output.constData());
    }

    void recognizesFromLocalInstallation()
    {
        if (!qEnvironmentVariableIsSet("FLAMESHOT_OCR_BUNDLE_TEST")) {
            return; // Invoked in a child process with an isolated installation.
        }
        qputenv("PATH", "/nonexistent");
        qunsetenv("TESSDATA_PREFIX");
        qputenv("FLAMESHOT_OCR_TEST_MODE", "bundled");
        OcrDialog dialog(capture());
        auto* copy = dialog.findChild<QPushButton*>("ocrCopy");
        QTRY_VERIFY_WITH_TIMEOUT(
          copy->isEnabled() || !status(dialog).startsWith("Recognizing"), 3000);
        QVERIFY2(copy->isEnabled(), qPrintable(status(dialog)));
        QCOMPARE(dialog.text(), QString::fromUtf8("中文识别\nHello OCR 123"));
    }

    void requiresBothLanguages()
    {
        qputenv("FLAMESHOT_OCR_TEST_MODE", "missing_language");
        OcrDialog dialog(capture());
        QTRY_VERIFY(status(dialog).contains("language data are required"));
        QVERIFY(recorded("image").isEmpty());
        QVERIFY(!dialog.findChild<QPushButton*>("ocrCopy")->isEnabled());
    }

    void handlesEmptyResult()
    {
        qputenv("FLAMESHOT_OCR_TEST_MODE", "empty");
        OcrDialog dialog(capture());
        QTRY_VERIFY(status(dialog).contains("No text was found"));
        QVERIFY(!dialog.findChild<QPushButton*>("ocrCopy")->isEnabled());
        QCOMPARE(QApplication::clipboard()->text(),
                 QString("original clipboard"));
    }

    void reportsEngineFailure()
    {
        qputenv("FLAMESHOT_OCR_TEST_MODE", "failure");
        OcrDialog dialog(capture());
        QTRY_VERIFY(status(dialog).contains("Invalid OCR image"));
        QVERIFY(!dialog.findChild<QPushButton*>("ocrCopy")->isEnabled());
    }

    void cancelStopsEngineAndRemovesImage()
    {
        qputenv("FLAMESHOT_OCR_TEST_MODE", "slow");
        QString imagePath;
        int pid = 0;
        {
            OcrDialog dialog(capture());
            dialog.show();
            QTRY_VERIFY(!recorded("pid").isEmpty());
            pid = recorded("pid").toInt();
            QVERIFY(pid > 0);
            imagePath = QString::fromUtf8(recorded("image"));
            QVERIFY(QFile::exists(imagePath));
            QTest::keyClick(&dialog, Qt::Key_Escape);
            QCOMPARE(dialog.result(), int(QDialog::Rejected));
        }
        QCOMPARE(::kill(pid, 0), -1);
        QCOMPARE(errno, ESRCH);
        QVERIFY(!QFile::exists(imagePath));
        QCOMPARE(QApplication::clipboard()->text(),
                 QString("original clipboard"));
    }

    void realEngineRecognizesChineseAndEnglish()
    {
        const QByteArray enginePath = qgetenv("FLAMESHOT_OCR_REAL_PATH");
        if (enginePath.isEmpty()) {
            QSKIP("Set FLAMESHOT_OCR_REAL_PATH to run the real Tesseract smoke "
                  "test.");
        }
        qputenv("PATH", enginePath);
        QPixmap image(1000, 220);
        image.fill(Qt::white);
        QPainter painter(&image);
        QFont font("Noto Sans CJK SC");
        font.setPixelSize(42);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(30, 80, QString::fromUtf8("截图文字识别"));
        painter.drawText(30, 160, "Hello OCR 123");
        painter.end();

        OcrDialog dialog(image);
        dialog.show();
        auto* copy = dialog.findChild<QPushButton*>("ocrCopy");
        QTRY_VERIFY_WITH_TIMEOUT(copy->isEnabled(), 20000);
        auto* text = dialog.findChild<QPlainTextEdit*>("ocrText");
        const QString result = text->toPlainText();
        QVERIFY2(result.contains("Hello OCR 123"), qPrintable(result));
        QString compact = result;
        compact.remove(' ');
        QVERIFY2(compact.contains(QString::fromUtf8("截图文字识别")),
                 qPrintable(result));
        const QString previewPath =
          qEnvironmentVariable("FLAMESHOT_OCR_PREVIEW");
        if (!previewPath.isEmpty()) {
            QVERIFY(dialog.grab().save(previewPath));
        }
        copy->click();
        QCOMPARE(dialog.text(), result);
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void timeoutStopsEngine()
    {
        qputenv("FLAMESHOT_OCR_TEST_MODE", "slow");
        OcrDialog dialog(capture());
        QTRY_VERIFY(!recorded("pid").isEmpty());
        const int pid = recorded("pid").toInt();
        QVERIFY(pid > 0);
        auto* timeout = dialog.findChild<QTimer*>("ocrTimeout");
        QVERIFY(timeout);
        timeout->start(10);
        QTRY_VERIFY(status(dialog).contains("timed out"));
        QTRY_COMPARE(::kill(pid, 0), -1);
        QCOMPARE(errno, ESRCH);
        QVERIFY(!dialog.findChild<QPushButton*>("ocrCopy")->isEnabled());
    }
};

QTEST_MAIN(OcrDialogTest)
#include "ocr_dialog_test.moc"
