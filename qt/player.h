/* GStreamer
 *
 * Copyright (C) 2015 Alexandre Moreno <alexmorenocano@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QQuickItem>
#include "qgstplayer.h"

class QuickRenderer;

class Player : public QGstPlayer
{
    Q_OBJECT
public:
    Player(QObject *parent = 0);
    void setVideoOutput(QQuickItem *output);

private:
    Player(QObject *parent, QuickRenderer *renderer);
    QuickRenderer *renderer_;
};

Q_DECLARE_METATYPE(Player*)

#endif // PLAYER_H
