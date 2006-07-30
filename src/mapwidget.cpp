/***************************************************************************
 *   Copyright (C) 2004-2005 by Albert Astals Cid                          *
 *   tsdgeos@terra.es                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <math.h> // for rint

#include <kapplication.h>
#include <klocale.h>

#include <qcursor.h>
#include <qevent.h>
#include <qpainter.h>

#include "mapwidget.h"

mapWidget::mapWidget(QWidget *parent) : QWidget(parent)
{
	p_wantZoom = false;
	p_zooming = false;
	p_wantMove = false;
	p_moving = false;
	p_zoomW = 0;
	p_zoomH = 0;
	p_scrollBarWidth = 0;
	p_scrollBarHeight = 0;
	p_scrollBarsVisible = 0;
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_StaticContents);
}

void mapWidget::init(const QString &path, int scrollBarWidth, int scrollBarHeight)
{
	p_scrollBarWidth = scrollBarWidth;
	p_scrollBarHeight = scrollBarHeight;
	p_originalImage.load(path);
	p_originalPixmap.load(path);
	emit updateMaximumSize(p_originalImage.width(), p_originalImage.height());
	setOriginalImage();
}

void mapWidget::setMapMove(bool b)
{
	p_wantMove = b;
	if (b)
	{
		emit setZoomActionChecked(false);
		p_wantZoom = false;
	}
}

void mapWidget::setMapZoom(bool b)
{
	p_wantZoom = b;
	if (b)
	{
		emit setMoveActionChecked(false);
		p_wantMove = false;
	}
}

QSize mapWidget::sizeHint() const
{
	return maximumSize();
}

void mapWidget::updateHPosition(int value)
{
	p_zoomX = value;
	updateShownImage();
}

void mapWidget::updateVPosition(int value)
{
	p_zoomY = value;
	updateShownImage();
}

void mapWidget::mousePressEvent(QMouseEvent *e)
{
	if (e -> button() == Qt::LeftButton)
	{
		if (p_wantZoom)
		{
			p_initial = e -> pos();
			p_zooming = true;
		}
		else if (p_wantMove)
		{
			p_initial = e -> pos();
			setCursor(Qt::SizeAllCursor);
			p_moving = true;
		}
		else
		{
			QRgb rgb;
			QImage *currentImage;
			
			currentImage = getCurrentImage();
			
			rgb = currentImage -> pixel(e -> x(), e -> y());
			emit clicked(rgb, e -> pos());
		}
	}
	else
	{
		if (p_wantZoom)
		{
			setOriginalImage();
		}
		else e->ignore(); // that makes the event go to mapasker and clear the popup
	}
}

void mapWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (p_zooming)
	{
		p_current = e -> pos();
		update();
	}
	else if (p_moving)
	{
		int oW, oH;
		int auxX, auxY;
		bool updatePos = false;
		
		// some shortcuts :-D 
		oW = p_originalImage.width();
		oH = p_originalImage.height();
		
		// where next x and y will be
		auxX = (int) ((p_initial.x() - e -> pos().x()) * p_lastFactorX);
		if (qAbs(auxX) > 0)
		{
			p_zoomX += auxX;
			updatePos = true;
		}
		
		auxY = (int) ((p_initial.y() - e -> pos().y()) * p_lastFactorY);
		if (qAbs(auxY) > 0)
		{
			p_zoomY += auxY;
			updatePos = true;
		}
		
		// make sure we don't go out of bounds
		if (p_zoomX < 0) p_zoomX = 0;
		if (p_zoomY < 0) p_zoomY = 0;
		if (p_zoomX > oW - width() * p_lastFactorX) p_zoomX = (int)rint(oW - width() * p_lastFactorX);
		if (p_zoomY > oH - height() * p_lastFactorY) p_zoomY = (int)rint(oH - height() * p_lastFactorY);
		
		if (updatePos) p_initial = e -> pos();
		
		updateShownImage();
		emit updatePosition(p_zoomX, p_zoomY);
	}
}

void mapWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (p_zooming)
	{
		QRect r(p_initial, e -> pos());
		
		r = r.normalized();
		
		p_zoomX += (int)rint(r.left() * p_lastFactorX);
		p_zoomY += (int)rint(r.top() * p_lastFactorY);
		
		p_zoomW = (int)rint(r.width() * p_lastFactorX);
		p_zoomH = (int)rint(r.height() * p_lastFactorY);
		
		if (r.right() > width()) p_zoomW = width() - p_zoomX;
		if (r.height() > height()) p_zoomH = height() - p_zoomY;
		if (p_zoomX < 0)
		{
			p_zoomX = 0;
			p_zoomW = (int)rint(r.right() * p_lastFactorX);
		}
		if (p_zoomY < 0)
		{
			p_zoomY = 0;
			p_zoomH = (int)rint(r.bottom() * p_lastFactorY);
		}
		
		p_zooming = false;
		
		if (p_zoomW > 1 && p_zoomH > 1)
		{
			double factorX, factorY;
			int maxX, maxY;
			
			updateShownImage();
			
			factorX = (double)p_zoomW / width();
			factorY = (double)p_zoomH / height();
			
			maxX = (int)rint(p_originalImage.width() / factorX);
			maxY = (int)rint(p_originalImage.height() / factorY);
			setMaximumSize(maxX, maxY);
			
			emit updateVisibleSize(p_zoomW, p_zoomH);
			emit updatePosition(p_zoomX, p_zoomY);
			
			p_lastFactorX = factorX;
			p_lastFactorY = factorY;
			
			emitMoveActionEnabled();
		}
		else update();
	}
	else if (p_moving)
	{
		unsetCursor();
		p_moving = false;
	}
}

void mapWidget::resizeEvent(QResizeEvent *e)
{
	if (p_originalImage.isNull()) return;
	
	p_zoomW = (int)rint(e -> size().width() * p_lastFactorX);
	p_zoomH = (int)rint(e -> size().height() * p_lastFactorY);
	
	emit updateVisibleSize(p_zoomW, p_zoomH);
	updateShownImage();
	emitMoveActionEnabled();
}

void mapWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.drawPixmap(0, 0, *getCurrentPixmap());
	if (p_zooming) p.drawRect(QRect(p_initial, p_current).normalized());
}

void mapWidget::emitMoveActionEnabled()
{
	int w, h;
	
	w = p_zoomW;
	h = p_zoomH;
	if (p_scrollBarsVisible)
	{
		w += p_scrollBarWidth;
		h += p_scrollBarHeight;
	}
	
	if (w < maximumWidth() * p_lastFactorX || h < maximumHeight() * p_lastFactorY)
	{
		p_scrollBarsVisible = true;
		emit setMoveActionEnabled(true);
	}
	else
	{
		p_scrollBarsVisible = false;
		emit setMoveActionChecked(false);
		emit setMoveActionEnabled(false);
	}
}

QImage *mapWidget::getCurrentImage()
{
	if (p_zoomedImageShown.isNull()) return &p_originalImage;
	else return &p_zoomedImageShown;
}

QPixmap *mapWidget::getCurrentPixmap()
{
	if (p_zoomedImageShown.isNull()) return &p_originalPixmap;
	else return &p_zoomedPixmapShown;
}

void mapWidget::setOriginalImage()
{
	p_zoomedImageShown = QImage();
	p_zoomedPixmapShown = QPixmap();
	p_lastFactorX = 1;
	p_lastFactorY = 1;
	p_zoomX = 0;
	p_zoomY = 0;
	
	if (p_zoomH != 0 && p_zoomW != 0)
	{
		/* setting the original image not when opening the app */
		p_zoomW = width();
		p_zoomH = height();
	}
	
	p_oldZoomX = p_zoomX;
	p_oldZoomY = p_zoomY;
	p_oldZoomW = p_zoomW;
	p_oldZoomH = p_zoomH;
	p_oldSize = size();
	
	emit updatePosition(0, 0);
	emit updateVisibleSize(p_zoomW, p_zoomH);
	setMaximumSize(p_originalImage.size());
	emitMoveActionEnabled();
	repaint();
}

void mapWidget::updateShownImage()
{
	if (p_originalImage.isNull()) return;

	if (p_oldZoomX != p_zoomX || p_oldZoomY != p_zoomY || p_oldZoomW != p_zoomW || p_oldZoomH != p_zoomH || size() != p_oldSize)
	{
		p_zoomedImageShown = p_originalImage.copy(p_zoomX, p_zoomY, p_zoomW, p_zoomH);
		p_zoomedImageShown = p_zoomedImageShown.scaled(size());
		p_zoomedPixmapShown = QPixmap::fromImage( p_zoomedImageShown );
		p_oldZoomX = p_zoomX;
		p_oldZoomY = p_zoomY;
		p_oldZoomW = p_zoomW;
		p_oldZoomH = p_zoomH;
		p_oldSize = size();
		repaint();
	}
}

QSize mapWidget::mapSize() const
{
	return p_originalImage.size();
}

#include "mapwidget.moc"
