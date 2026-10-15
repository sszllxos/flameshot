// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QPixmap>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>

class QLabel;
class QPlainTextEdit;
class QPushButton;

class OcrDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OcrDialog(const QPixmap& capture, QWidget* parent = nullptr);
    ~OcrDialog() override;
    QString text() const;

private:
    void start(const QPixmap& capture);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void fail(const QString& message);

    QTemporaryFile m_inputFile;
    QProcess m_process;
    QTimer m_timeout;
    QString m_executable;
    QLabel* m_status;
    QPlainTextEdit* m_text;
    QPushButton* m_copy;
    bool m_checkingLanguages = true;
    bool m_done = false;
};
