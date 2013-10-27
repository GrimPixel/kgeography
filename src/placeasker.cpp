/***************************************************************************
 *   Copyright (C) 2004-2007 by Albert Astals Cid                          *
 *   aacid@kde.org                                                         *
 *   Copyright (C) 2006 by Isaac Clerencia                                 *
 *   isaac@warp.es                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "placeasker.h"

#include <cmath>
#include <math.h>

#include <klocale.h>
#include <kmessagebox.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qscrollbar.h>
#include <qstring.h>

#include "map.h"
#include "placemapwidget.h"

placeAsker::placeAsker(QWidget *parent, KGmap *m, QWidget *w, uint count) : askWidget(parent, m, w, count, true), p_firstShow(true), p_currentDivisionImage(0)
{
	QVBoxLayout *lay = new QVBoxLayout(this);
	lay -> setMargin(0);
	lay -> setSpacing(0);

	p_mapImage = new QImage(p_map->getMapFile());
	p_mapWidget = new placeMapWidget(this);
	lay -> addWidget(p_mapWidget);
	
	connect(p_mapWidget, SIGNAL(clicked(QRgb, const QPoint&, const QPointF&)), this, SLOT(handleMapClick(QRgb, const QPoint&, const QPointF&)));
	connect(p_mapWidget, SIGNAL(setMoveActionChecked(bool)), this, SIGNAL(setMoveActionChecked(bool)));
	connect(p_mapWidget, SIGNAL(setZoomActionChecked(bool)), this, SIGNAL(setZoomActionChecked(bool)));
	connect(p_mapWidget, SIGNAL(setMoveActionEnabled(bool)), this, SIGNAL(setMoveActionEnabled(bool)));
	
	QVBoxLayout *vbl = static_cast<QVBoxLayout*>(w -> layout());
	p_next = new QLabel(w);
	p_next -> setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	p_next -> setWordWrap(true);
	p_fill = new QWidget(w);
	p_fill -> show();
	vbl -> addWidget(p_next);
	vbl -> addWidget(p_fill, 1);
	p_placedPixelIndices = p_mapWidget -> outerPixelIndices();
	nextQuestion();
}

placeAsker::~placeAsker()
{
	delete p_next;
	delete p_fill;
	delete p_mapImage;
}

bool placeAsker::isAsker() const
{
	return p_answers;
}

void placeAsker::mousePressEvent(QMouseEvent*)
{
}

void placeAsker::setMovement(bool b)
{
	p_mapWidget -> setMapMove(b);
}

void placeAsker::setZoom(bool b)
{
	askWidget::setZoom(b);
	p_mapWidget -> setMapZoom(b);
}

void placeAsker::setOriginalZoom()
{
	p_mapWidget -> setGameImage();
}

void placeAsker::setAutomaticZoom(bool automaticZoom)
{
	p_mapWidget -> setAutomaticZoom(automaticZoom);
}

void placeAsker::handleMapClick(QRgb c, const QPoint & , const QPointF &mapPoint)
{
	QString aux, cap;
	aux = p_map -> getWhatIs(c, false);
	if (aux == "nothing") KMessageBox::error(this, i18nc("@info", "You have found a bug in a map. Please contact the author and tell the %1 map has nothing associated to color %2,%3,%4.", p_map -> getFile(), qRed(c), qGreen(c), qBlue(c)));
	else
	{
		p_mapWidget->placeDivision(p_currentDivisionImage, p_currentDivisionRect);
		p_mapWidget->unsetCursor();
		// the image is no longer needed
		delete p_currentDivisionImage;

		double distX = p_currentDivisionRect.center().x() - mapPoint.x();
		double distY = p_currentDivisionRect.center().y() - mapPoint.y();
		double distance = sqrt((double)distX * distX + distY * distY); 
		int indexOfCurrent = p_mapImage -> colorTable().indexOf(p_currentRgb);
		bool consideredGood = distance < 5.0;
		// if we consider it good enough don't transmit a may be wrong color
		if (consideredGood) c = p_currentRgb;
		if (! consideredGood)
		{
			bool hasBorderShown = false;
			for ( int i = p_placedPixelIndices.size() ; --i >= 0 && !hasBorderShown ; )
			{
				uchar pixelIndex = p_placedPixelIndices[i];
				size_t nb = p_mapWidget -> nbBorderPixels(pixelIndex, indexOfCurrent);
				hasBorderShown = nb > 3;
			}
			consideredGood = !hasBorderShown && distance < 16.0;
			if (consideredGood) c = p_currentRgb;
		}
		if (! consideredGood)
		{
			QRect definedRect(0, 0, p_mapImage -> width(), p_mapImage -> height());
			QPoint v = QPoint(mapPoint.x(), mapPoint.y()) - p_currentDivisionRect.center();
			QRect initialRect(p_currentDivisionRect);
			QRect userRect = initialRect.translated(v);
			QRect definedRectUser = userRect & definedRect;
			QPoint definedFirstDiag = definedRectUser.bottomRight() - definedRectUser.topLeft();
			QPoint origFirstDiag = userRect.bottomRight() - userRect.topLeft();
			QPoint badDiff = origFirstDiag -definedFirstDiag;
			QPoint diagDiff = origFirstDiag -badDiff;
			QVector<size_t> stats(p_mapImage -> colorTable().size());
			size_t goodCount = 0;
			size_t outCount = badDiff.x() * badDiff.y() + badDiff.x() * diagDiff.y() + diagDiff.x() * badDiff.y();
			size_t badCount = outCount;
			for ( int dy = definedFirstDiag.y() -1 ; dy >= 0 ; dy-- )
			{
				for ( int dx = definedFirstDiag.x() -1 ; dx >= 0 ; dx-- )
				{
					int origPixelIndex = p_mapImage -> pixelIndex(initialRect.left() + dx, initialRect.top() + dy);
					if ( origPixelIndex != indexOfCurrent )
						continue;
					int userPixelIndex = p_mapImage -> pixelIndex(definedRectUser.left() + dx, definedRectUser.top() + dy);
					if ( userPixelIndex == origPixelIndex ) goodCount++;
					else
					{
						stats[userPixelIndex]++;
						badCount++;
					}
				}
			}
			consideredGood = goodCount > 0.5 * (goodCount + badCount);
			if (consideredGood) c = p_currentRgb;
			else if (outCount > 0.5 * (goodCount + badCount))
			{
				c = p_map -> getIgnoredDivisions(askMode())[0] -> getRGB();
			}
			else
			{
				int indexOfMax = -1;
				size_t maxCount = 0;
				for ( int i = stats.size() -1 ; i >= 0 ; i-- )
				{
					if ( stats[i] > maxCount )
					{
						indexOfMax = i;
						maxCount = stats[i];
					}
				}
				c = p_mapImage -> colorTable()[indexOfMax];
			}
		}
		p_placedPixelIndices.append(indexOfCurrent);
		p_currentAnswer.setAnswer(QColor(c));
		questionAnswered(consideredGood);
		nextQuestion();
	}
}

void placeAsker::nextQuestionHook(const QString &division)
{
	QString divisionName = i18nc(p_map -> getFileName().toUtf8(), division.toUtf8());
	p_next -> setText(i18nc("@info:status", "Please place in the map:<br/><b>%1</b>", divisionName));
	p_next -> show();
	p_currentAnswer.setQuestion(i18nc("@item:intable column Question, %1 is region name", "%1", i18nc(p_map -> getFileName().toUtf8(), division.toUtf8())));
	QColor color = p_map -> getColor(division);
	p_currentRgb = color.rgb();
	p_currentAnswer.setCorrectAnswer(color);
	setCurrentDivision(division);
	p_mapWidget->setCurrentDivisionImage(p_currentDivisionImage);
}

QString placeAsker::getQuestionHook() const
{
	QString divisionType = i18nc(p_map -> getFileName().toUtf8(), p_map->getDivisionsString().toUtf8());
	return i18nc("@title", "Place %1 in Map", divisionType);
}

void placeAsker::showEvent(QShowEvent *)
{
	if (p_firstShow)
	{
		p_mapWidget -> init(p_map, p_mapImage);
		p_firstShow = false;
	}
}

QSize placeAsker::mapSize() const
{
	return p_mapWidget -> mapSize();
}

void placeAsker::setCurrentDivision(const QString& division)
{
	int width = p_mapImage->width();
	int height = p_mapImage->height();

	int minX = width;
	int maxX = 0;
	int minY = height;
	int maxY = 0;

	QRgb divColor = p_map->getColor(division).rgb();

	//first iteration, detect size required by the image
	for (int x = 0; x < width; x++)
	{
		for (int y = 0; y < height; y++)
		{
			if (p_mapImage->pixel(x,y) == divColor)
			{
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}
	}

	p_currentDivisionImage = new QImage(maxX - minX, maxY - minY, QImage::Format_ARGB32);
	p_currentDivisionRect.setCoords(minX, minY, maxX, maxY);
	QColor transparent(0,0,0,0);
	p_currentDivisionImage->fill(transparent.rgba());

	//second iteration, copy the color to the new image
	for (int x = minX; x < maxX; x++)
	{
		for (int y = minY; y < maxY; y++)
		{
			if (p_mapImage->pixel(x,y) == divColor)
				p_currentDivisionImage->setPixel(x - minX, y - minY, divColor);
		}
	}
}

#include "placeasker.moc"
