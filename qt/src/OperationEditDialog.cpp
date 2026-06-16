#include "camclaw/qt/OperationEditDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace camclaw {

namespace {

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.c_str());
}

std::string toStd(const QString& value)
{
    return value.toUtf8().constData();
}

} // namespace

OperationEditDialog::OperationEditDialog(const CamObject& operation, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("operationEditDialog");
    setWindowTitle(QString::fromUtf8("编辑工序"));
    buildUi(operation);
}

QString OperationEditDialog::parameterValue(const QString& name) const
{
    if (!edits_.contains(name)) {
        return QString();
    }
    return edits_[name]->text();
}

std::map<std::string, std::string> OperationEditDialog::editedParameters() const
{
    std::map<std::string, std::string> parameters;
    for (QMap<QString, QLineEdit*>::const_iterator it = edits_.begin(); it != edits_.end(); ++it) {
        parameters[toStd(it.key())] = toStd(it.value()->text());
    }
    return parameters;
}

void OperationEditDialog::buildUi(const CamObject& operation)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    QLabel* title = new QLabel(fromStd(operation.display_name) + QString::fromUtf8(" / ") + fromStd(operation.object_id), this);
    title->setObjectName("operationEditTitle");
    root->addWidget(title);

    QFormLayout* form = new QFormLayout();
    root->addLayout(form);

    const char* names[] = { "tool_id", "stepover", "stepdown", "tolerance", "toolpath_status" };
    for (std::size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        const std::map<std::string, std::string>::const_iterator found = operation.attributes.find(names[index]);
        addParameterRow(QString::fromUtf8(names[index]), found == operation.attributes.end() ? QString() : fromStd(found->second));
        form->addRow(QString::fromUtf8(names[index]), edits_[QString::fromUtf8(names[index])]);
    }

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("operationEditButtons");
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    root->addWidget(buttons);
}

void OperationEditDialog::addParameterRow(const QString& name, const QString& value)
{
    QLineEdit* edit = new QLineEdit(value, this);
    edit->setObjectName("operationParam_" + name);
    edits_[name] = edit;
}

} // namespace camclaw
