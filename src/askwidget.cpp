/***************************************************************************
 *   Copyright (C) 2004 by Albert Astals Cid                               *
 *   tsdgeos@terra.es                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "askwidget.h"

askWidget::askWidget(QWidget *parent, map *m, uint count) : QWidget(parent), p_map(m), p_count(count)
{
}

askWidget::~askWidget()
{
}

#include "askwidget.moc"
