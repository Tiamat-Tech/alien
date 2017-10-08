#pragma once

#include <QTextEdit>
#include "Gui/Definitions.h"

class MetadataPropertiesEditWidget
	: public QTextEdit
{
    Q_OBJECT
public:
    MetadataPropertiesEditWidget(QWidget *parent = 0);
	virtual ~MetadataPropertiesEditWidget() = default;

	void init(DataEditorModel* model, DataEditorController* controller);
	void updateDisplay();

private:
    Q_SLOT void keyPressEvent (QKeyEvent* e);
	Q_SLOT void mousePressEvent (QMouseEvent* e);
	Q_SLOT void mouseDoubleClickEvent (QMouseEvent* e);

	void updateModel();

	DataEditorModel* _model = nullptr;
	DataEditorController* _controller = nullptr;
};
