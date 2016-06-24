/*****************************************************************************
 * RendererEventWatcher.h: Watches events from a IConsumer and convert them to SIGNAL
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef RENDEREREVENTWATCHER_H
#define RENDEREREVENTWATCHER_H

#include <QObject>
#include "Backend/VLC/VLCSourceRenderer.h"

class RendererEventWatcher : public QObject, public Backend::ISourceRendererEventCb
{
    Q_OBJECT
public:
    explicit RendererEventWatcher(QObject *parent = 0);

private:
    virtual void    onTimeChanged( int64_t );
    virtual void    onPlaying();
    virtual void    onPaused();
    virtual void    onStopped();
    virtual void    onEndReached();
    virtual void    onVolumeChanged();
    virtual void    onPositionChanged( float );
    virtual void    onLengthChanged( int64_t );
    virtual void    onErrorEncountered();

signals:
    void            timeChanged( qint64 );
    void            playing();
    void            paused();
    void            stopped();
    void            endReached();
    void            volumeChanged();
    void            positionChanged( float );
    void            lengthChanged( qint64 );
    void            errorEncountered();
};

#endif // RENDEREREVENTWATCHER_H
