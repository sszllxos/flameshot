// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrtool.h"
#include "core/flameshotdaemon.h"
#include "ocrdialog.h"

#include <QApplication>

OcrTool::OcrTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool OcrTool::closeOnButtonPressed() const
{
    return false;
}

QIcon OcrTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "ocr.svg");
}

QString OcrTool::name() const
{
    return tr("Recognize Text");
}

QString OcrTool::description() const
{
    return tr("Recognize Chinese and English text in the selection (offline)");
}

CaptureTool::Type OcrTool::type() const
{
    return CaptureTool::TYPE_OCR;
}

CaptureTool* OcrTool::copy(QObject* parent)
{
    return new OcrTool(parent);
}

void OcrTool::pressed(CaptureContext& context)
{
    emit requestAction(REQ_CLEAR_SELECTION);
    OcrDialog dialog(context.selectedScreenshotArea(),
                     QApplication::activeWindow());
    if (dialog.exec() == QDialog::Accepted) {
        FlameshotDaemon::copyToClipboard(dialog.text(),
                                         tr("Text copied to clipboard."));
        emit requestAction(REQ_CAPTURE_DONE_OK);
        emit requestAction(REQ_CLOSE_GUI);
    }
}
