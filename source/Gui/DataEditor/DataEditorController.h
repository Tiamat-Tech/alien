#pragma once

#include <QObject>
#include "Gui/Definitions.h"

class DataEditorController
	: public QObject
{
	Q_OBJECT
public:
	DataEditorController(QObject *parent = nullptr);
	virtual ~DataEditorController() = default;

	DataEditorContext* getContext() const;

private:
	DataEditorModel* _model = nullptr;
	DataEditorContext* _context = nullptr;
};