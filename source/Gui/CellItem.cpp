#include <QPainter>
#include <QGraphicsBlurEffect>

#include "EngineInterface/ChangeDescriptions.h"
#include "EngineInterface/Colors.h"

#include "Gui/Settings.h"

#include "ItemConfig.h"
#include "CellItem.h"
#include "CoordinateSystem.h"
    
namespace
{
	QString getTypeString(Enums::CellFunction::Type type)
	{
		if (type == Enums::CellFunction::COMPUTER)
			return "Computer";
		else if (type == Enums::CellFunction::PROPULSION)
			return "Propulsion";
		else if (type == Enums::CellFunction::SCANNER)
			return "Scanner";
		else if (type == Enums::CellFunction::WEAPON)
			return "Weapon";
		else if (type == Enums::CellFunction::CONSTRUCTOR)
			return "Constructor";
		else if (type == Enums::CellFunction::SENSOR)
			return "Sensor";
		else if (type == Enums::CellFunction::MUSCLE)
			return "Muscle";
		else
			return QString();
	}

}

CellItem::CellItem (ItemConfig* config, CellDescription const& desc, QGraphicsItem* parent /*= nullptr*/)
    : AbstractItem(parent), _config(config)
{
	update(desc);
}

void CellItem::update(CellDescription const &desc)
{
	_desc = desc;
	auto pos = CoordinateSystem::modelToScene(*desc.pos);
	QGraphicsItem::setPos(QPointF(pos.x(), pos.y()));
	_displayString = getTypeString(_desc.cellFeature->getType());
}

QRectF CellItem::boundingRect () const
{
    return QRectF(CoordinateSystem::modelToScene(-0.5), CoordinateSystem::modelToScene(-0.5)
		, CoordinateSystem::modelToScene(1.0), CoordinateSystem::modelToScene(1.0));
}

void CellItem::paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //set brush color
	QColor color;
	uint8_t colorCode = getColorCode() % 7;
    if (colorCode == 0)
        color = toQColor(Const::IndividualCellColor1);
    if( colorCode == 1 )
        color = toQColor(Const::IndividualCellColor2);
    if( colorCode == 2 )
        color = toQColor(Const::IndividualCellColor3);
    if( colorCode == 3 )
        color = toQColor(Const::IndividualCellColor4);
    if( colorCode == 4 )
        color = toQColor(Const::IndividualCellColor5);
    if( colorCode == 5 )
        color = toQColor(Const::IndividualCellColor6);
    if( colorCode == 6 )
        color = toQColor(Const::IndividualCellColor7);

    adaptColorForFocus(color);
    painter->setBrush(QBrush(color));

    //boundary color (pen)
    adaptColorForBoundary(color);
    painter->setPen(QPen(QBrush(color), CoordinateSystem::modelToScene(0.05)));

    //draw cell
    if( (_focusState == NO_FOCUS) || (_focusState == FOCUS_CLUSTER) )
        painter->drawEllipse(QPointF(0.0, 0.0), CoordinateSystem::modelToScene(0.33), CoordinateSystem::modelToScene(0.33));
    else
        painter->drawEllipse(QPointF(0.0, 0.0), CoordinateSystem::modelToScene(0.43), CoordinateSystem::modelToScene(0.43));

    //draw token
	int numToken = getNumToken();
    if( numToken > 0 ) {

        auto tokenColor = Const::TokenColor;
        adaptColorForFocus(color);
        painter->setBrush(QBrush(tokenColor));
        adaptColorForBoundary(tokenColor);
        painter->setPen(QPen(QBrush(tokenColor), CoordinateSystem::modelToScene(0.03)));
        qreal shift1 = -0.5*0.20*(qreal)(numToken-1);
        if( numToken > 3)
            shift1 = -0.5*0.20*2.0;
        qreal shiftY1 = -0.5*0.35*(qreal)((numToken-1)/3);
        for( int i = 0; i < numToken; ++i) {
            qreal shift2 = 0.20*(qreal)(i%3);
            qreal shiftY2 = 0.35*(qreal)(i/3);
            if( numToken <= 3 )
                painter->drawEllipse(CoordinateSystem::modelToScene(QPointF(shift1+shift2, shift1+shift2+shiftY1+shiftY2)), CoordinateSystem::modelToScene(0.25), CoordinateSystem::modelToScene(0.15));
            else
                painter->drawEllipse(CoordinateSystem::modelToScene(QPointF(shift1+shift2, shift1+shift2+shiftY1+shiftY2)), CoordinateSystem::modelToScene(0.25), CoordinateSystem::modelToScene(0.15));
        }
    }

	if (_config->isShowCellInfo()) {
		auto font = GuiSettings::getCellFont();
		painter->setFont(font);
		painter->setPen(QPen(QBrush(toQColor(Const::CellFunctionInfoColor)), CoordinateSystem::modelToScene(0.03)));
		painter->drawText(QRectF(CoordinateSystem::modelToScene(-1.5), CoordinateSystem::modelToScene(0.1), CoordinateSystem::modelToScene(3.0)
			, CoordinateSystem::modelToScene(1.0)), Qt::AlignCenter, _displayString);
		painter->setPen(QPen(QBrush(toQColor(Const::BranchNumberInfoColor)), CoordinateSystem::modelToScene(0.03)));
		painter->drawText(QRectF(CoordinateSystem::modelToScene(-0.49), CoordinateSystem::modelToScene(-0.47)
			, CoordinateSystem::modelToScene(1.0), CoordinateSystem::modelToScene(1.0)), Qt::AlignCenter, QString::number(getBranchNumber()));
	}

}

int CellItem::type() const
{
    // enables the use of qgraphicsitem_cast with this item.
    return Type;
}

uint64_t CellItem::getId() const
{
	return _desc.id;
}

list<uint64_t> CellItem::getConnectedIds() const
{
    list<uint64_t> result;
    if (!_desc.connections) {
        return result;
    }
    for (auto const& connection : *_desc.connections) {
        result.emplace_back(connection.cellId);
    }
	return result;
}

CellItem::FocusState CellItem::getFocusState ()
{
    return _focusState;
}

void CellItem::setFocusState (FocusState focusState)
{
    _focusState = focusState;
}

void CellItem::setDisplayString(QString value)
{
	_displayString = value;
}

void CellItem::adaptColorForFocus(QColor& color) const
{
    int h, s, l;
    color.getHsl(&h, &s, &l);

    //fill color (brush)
    if (!isConnectable()) {
        l = std::max(0, l - 30);
    }
    if (CellItem::FOCUS_CELL == _focusState) {
        l = 190;
    }
    if (CellItem::FOCUS_CLUSTER == _focusState) {
        l = std::min(255, l + 20);
    }
    color.setHsl(h, s, l);
}

void CellItem::adaptColorForBoundary(QColor& color) const
{
    int h, s, l;
    color.getHsl(&h, &s, &l);
    if (_focusState == NO_FOCUS) {
        l = std::max(0, l - 60);
        color.setHsl(h, s, l);
    }
    if (_focusState == FOCUS_CLUSTER) {
        l = std::min(255, l + 15);
        color.setHsl(h, s, l);
    }
    if (_focusState == FOCUS_CELL) {
        color.setHsl(h, s, 255);
    }
}

int CellItem::getBranchNumber() const
{
	return _desc.tokenBranchNumber.get_value_or(0);
}

int CellItem::getNumToken() const
{
	return _desc.tokens.get_value_or(vector<TokenDescription>()).size();
}

bool CellItem::isConnectable() const
{
	auto numConnections = _desc.connections.get_value_or(list<ConnectionDescription>()).size();
	auto maxConnections = _desc.maxConnections.get_value_or(0);
	return (numConnections < maxConnections);
}

uint8_t CellItem::getColorCode() const
{
	return _desc.metadata.get_value_or(CellMetadata()).color;
}

