/***************************************************************************
 *   Copyright (C) 2004-2005 by Albert Astals Cid                          *
 *   tsdgeos@terra.es                                                      *
 *   Copyright (C) 2006 by Isaac Clerencia                                 *
 *   isaac@warp.es
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <klocale.h>
#include <kmessagebox.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qscrollbar.h>
#include <qstring.h>
#include <QtDebug>

#include "map.h"
#include "placeasker.h"
#include "placemapwidget.h"

placeAsker::placeAsker(QWidget *parent, KGmap *m, QWidget *w, bool asker, uint count) : askWidget(parent, m, w, count, asker), p_popupManager(this), p_asker(asker), p_firstShow(true), p_currentDivisionImage(0)
{
	QVBoxLayout *lay = new QVBoxLayout(this);
	lay -> setMargin(0);
	lay -> setSpacing(0);

	p_mapImage = new QImage(p_map->getMapFile());
	p_mapWidget = new placeMapWidget(this);
	lay -> addWidget(p_mapWidget);
	
	p_shouldClearPopup = false;
	
	connect(p_mapWidget, SIGNAL(clicked(QRgb, const QPoint&)), this, SLOT(handleMapClick(QRgb, const QPoint&)));
	connect(p_mapWidget, SIGNAL(setMoveActionChecked(bool)), this, SIGNAL(setMoveActionChecked(bool)));
	connect(p_mapWidget, SIGNAL(setZoomActionChecked(bool)), this, SIGNAL(setZoomActionChecked(bool)));
	connect(p_mapWidget, SIGNAL(setMoveActionEnabled(bool)), this, SIGNAL(setMoveActionEnabled(bool)));
	
	if (asker)
	{
		QVBoxLayout *vbl = static_cast<QVBoxLayout*>(w -> layout());
		p_next = new QLabel(w);
		p_next -> setAlignment(Qt::AlignTop | Qt::AlignHCenter);
		p_fill = new QWidget(w);
		p_fill -> show();
		vbl -> addWidget(p_next);
		vbl -> addWidget(p_fill, 1);
		nextQuestion();
	}
	else
	{
		p_next = 0;
		p_fill = 0;
	}
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
	p_popupManager.clear();
}

void placeAsker::setMovement(bool b)
{
	p_mapWidget -> setMapMove(b);
	p_popupManager.clear();
}

void placeAsker::setZoom(bool b)
{
	p_mapWidget -> setMapZoom(b);
	p_popupManager.clear();
}

void placeAsker::setOriginalZoom()
{
	p_mapWidget -> setGameImage();
	p_popupManager.clear();
}

void placeAsker::setAutomaticZoom()
{
	p_mapWidget -> setAutomaticZoom();
	p_popupManager.clear();
}

void placeAsker::handleMapClick(QRgb c, const QPoint &p)
{
	QString aux, cap;
	aux = p_map -> getWhatIs(c, !p_asker);
	if (aux == "nothing") KMessageBox::error(this, i18n("You have found a bug in a map. Please contact the author and tell the %1 map has nothing associated to color %2,%3,%4.", p_map -> getFile(), qRed(c), qGreen(c), qBlue(c)));
	else if (p_shouldClearPopup)
	{
		p_popupManager.clear();
		p_shouldClearPopup = false;
	}
	else if (!p_asker)
	{
		QString flagFile;
		cap = p_map -> getDivisionCapital(aux);
		if (!cap.isEmpty())
		{
			flagFile = p_map -> getDivisionFlagFile(aux);
			cap = i18nc(p_map -> getFileName().toUtf8(), cap.toUtf8());
		}
		
		aux = i18nc(p_map -> getFileName().toUtf8(), aux.toUtf8());

		if (!flagFile.isEmpty()) p_popupManager.show(aux, cap, p, flagFile);
		else if (!cap.isEmpty()) p_popupManager.show(aux, cap, p);
		else p_popupManager.show(aux, p);
	}
	else if (!aux.isEmpty())
	{
		p_mapWidget->placeDivision(p_currentDivisionImage, p_currentDivisionRect);
		// the image is no longer needed
		delete p_currentDivisionImage;

		p_currentAnswer.setAnswer(QColor(c));
		questionAnswered(aux == lastDivisionAsked());
		nextQuestion();
	}
}

void placeAsker::nextQuestionHook(const QString &division)
{
	QString sw = i18nc("There are two ways of dealing with the translation of \"Please click on: %1\". The first option simply replaces %1 with the translated name of the relevant region/city. If the grammar of your language allows this, choose this option by setting the translation of this message to 1, and leave untranslated the translations of \"Please click on: %1\" that have the placename embedded (or translate them as - if you wish to show the file as fully translated. The second option is to translate all messages in full - this is likely to be required in the case of highly-inflected languages like Russian. To choose this option, set the translation of this message to 0, and translate all the messages.", "0");
	if (sw == "1")
	{
		QString divisionName = i18nc(p_map -> getFileName().toUtf8(), division.toUtf8());
		p_next -> setText(i18n("Please place in the map:\n%1", divisionName));
	}
	else
	{
		QString s = QString("Please place in the map:\n%1").arg(division);
		p_next -> setText(i18nc(p_map -> getFile().toUtf8(), s.toUtf8()));
	}
	p_next -> show();
	p_currentAnswer.setQuestion(i18nc(p_map -> getFile().toUtf8(), division.toUtf8()));
	p_currentAnswer.setCorrectAnswer(p_map -> getColor(division));
	setCurrentDivision(division);
	p_mapWidget->setCurrentDivisionImage(p_currentDivisionImage);
}

QString placeAsker::getQuestionHook() const
{
	return i18n("Place division in map");
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
				p_currentDivisionImage->setPixel(x - minX, y - minY, p_mapImage->pixel(x,y));
		}
	}
}

#include "placeasker.moc"