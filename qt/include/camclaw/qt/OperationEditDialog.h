#ifndef CAMCLAW_QT_OPERATION_EDIT_DIALOG_H
#define CAMCLAW_QT_OPERATION_EDIT_DIALOG_H

#include "camclaw/AgentWorkflowService.h"

#include <QDialog>
#include <QLineEdit>
#include <QMap>

namespace camclaw {

class OperationEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit OperationEditDialog(const CamObject& operation, QWidget* parent = 0);

    QString parameterValue(const QString& name) const;
    std::map<std::string, std::string> editedParameters() const;

private:
    void buildUi(const CamObject& operation);
    void addParameterRow(const QString& name, const QString& value);

    QMap<QString, QLineEdit*> edits_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_OPERATION_EDIT_DIALOG_H
