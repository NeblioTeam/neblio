#include "NTP1MetadataViewer.h"

#include <QMessageBox>

NTP1MetadataViewer::NTP1MetadataViewer(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Metadata viewer");

    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    resize(620, 500);

    mainLayout = new QGridLayout(this);
    setLayout(mainLayout);

    treeView = new JsonTreeView(this);
    textView = new QPlainTextEdit(this);
    treeView->setModel(&treeModel);
    switchTreeTextViewButton = new QPushButton("Switch tree/text view", this);

    textView->setReadOnly(true);
    textView->setVisible(false);

    mainLayout->addWidget(treeView, 0, 0, 1, 1);
    mainLayout->addWidget(textView, 0, 0, 1, 1);
    mainLayout->addWidget(switchTreeTextViewButton, 1, 0, 1, 1);

    connect(switchTreeTextViewButton, &QPushButton::clicked, this,
            &NTP1MetadataViewer::slot_switchTreeTextView);
}

void NTP1MetadataViewer::setRoot(std::shared_ptr<JsonTreeNode> root) { treeModel.setRoot(root); }

void NTP1MetadataViewer::setJsonStr(const std::string& jsonStr)
{
    json_spirit::Value v;
    try {
        json_spirit::read(jsonStr, v);
        setRoot(JsonTreeNode::ImportFromJson(v));
        textView->setPlainText(QString::fromStdString(jsonStr));
    } catch (std::exception& ex) {
        QMessageBox::warning(this, "Failed to read metadata",
                             "Failed to read transaction metadata. Error: " + QString(ex.what()));
    }
}

void NTP1MetadataViewer::slot_switchTreeTextView()
{
    bool treeVisible = treeView->isVisible();
    treeView->setVisible(!treeVisible);
    textView->setVisible(treeVisible);
}
