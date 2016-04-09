/*****************************************************************************
 * TrackHandler.cpp : Handle multiple track of a kind (audio or video)
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

#include "TrackHandler.h"
#include "TrackWorkflow.h"
#include "Workflow/Types.h"

#include <QDomDocument>
#include <QDomElement>

TrackHandler::TrackHandler( unsigned int nbTracks, Workflow::TrackType trackType ) :
        m_trackCount( nbTracks ),
        m_trackType( trackType ),
        m_length( 0 )
{
    m_tracks = new Toggleable<TrackWorkflow*>[nbTracks];
    for ( unsigned int i = 0; i < nbTracks; ++i )
    {
        m_tracks[i].setPtr( new TrackWorkflow( trackType, i ) );
        connect( m_tracks[i], SIGNAL( lengthChanged( qint64 ) ),
                 this, SLOT( lengthUpdated(qint64) ) );
    }
}

TrackHandler::~TrackHandler()
{
    for (unsigned int i = 0; i < m_trackCount; ++i)
        delete m_tracks[i];
    delete[] m_tracks;
}

void
TrackHandler::startRender( quint32 width, quint32 height )
{
    m_endReached = false;
    if ( m_length == 0 )
        m_endReached = true;
    else
    {
        for ( unsigned int i = 0; i < m_trackCount; ++i )
        {
            m_tracks[i]->initRender( width, height );
        }
    }
}

qint64
TrackHandler::getLength() const
{
    return m_length;
}

Workflow::OutputBuffer*
TrackHandler::getOutput( qint64 currentFrame, qint64 subFrame, bool paused )
{
    bool        validTrack = false;

    for ( int i = m_trackCount - 1; i >= 0; --i )
    {
        if ( m_tracks[i].activated() == false || m_tracks[i]->hasNoMoreFrameToRender( currentFrame ) )
            continue ;
        validTrack = true;
        Workflow::OutputBuffer  *ret = m_tracks[i]->getOutput( currentFrame, subFrame, paused );
        if ( ret == nullptr )
            continue ;
        else
            return ret;
    }
    if ( validTrack == false )
        allTracksEnded();
    return nullptr;
}

qint64
TrackHandler::getClipPosition( const QUuid &uuid, unsigned int trackId ) const
{
    Q_ASSERT( trackId < m_trackCount );

    return m_tracks[trackId]->getClipPosition( uuid );
}

void
TrackHandler::stop()
{
    for (unsigned int i = 0; i < m_trackCount; ++i)
        m_tracks[i]->stop();
}

void
TrackHandler::muteTrack( unsigned int trackId )
{
    m_tracks[trackId].deactivate();
}

void
TrackHandler::unmuteTrack( unsigned int trackId )
{
    m_tracks[trackId].activate();
}

ClipHelper*
TrackHandler::getClipHelper( const QUuid& uuid, unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );

    return m_tracks[trackId]->getClipHelper( uuid );
}

void
TrackHandler::clear()
{
    for ( unsigned int i = 0; i < m_trackCount; ++i )
    {
        m_tracks[i]->clear();
    }
    m_length = 0;
}

bool
TrackHandler::endIsReached() const
{
    return m_endReached;
}

void
TrackHandler::allTracksEnded()
{
    m_endReached = true;
    emit tracksEndReached();
}

unsigned int
TrackHandler::getTrackCount() const
{
    return m_trackCount;
}

void
TrackHandler::save( QXmlStreamWriter& project ) const
{
    for ( unsigned int i = 0; i < m_trackCount; ++i)
    {
        if ( m_tracks[i]->getLength() > 0 || m_tracks[i]->count( Effect::Filter ) > 0 )
        {
            project.writeStartElement( "track" );
            project.writeAttribute( "type", QString::number( (int)m_trackType ) );
            project.writeAttribute( "id", QString::number( i ) );
            m_tracks[i]->save( project );
            project.writeEndElement();
        }
    }
}

void
TrackHandler::renderOneFrame()
{
    for ( unsigned int i = 0; i < m_trackCount; ++i)
    {
        if ( m_tracks[i].activated() == true )
            m_tracks[i]->renderOneFrame();
    }
}

void
TrackHandler::setFullSpeedRender( bool val )
{
    for ( unsigned int i = 0; i < m_trackCount; ++i)
        m_tracks[i]->setFullSpeedRender( val );
}

void
TrackHandler::muteClip( const QUuid &uuid, quint32 trackId )
{
    m_tracks[trackId]->muteClip( uuid );
}

void
TrackHandler::unmuteClip( const QUuid &uuid, quint32 trackId )
{
    m_tracks[trackId]->unmuteClip( uuid );
}

bool
TrackHandler::contains( const QUuid &uuid ) const
{
    for ( unsigned int i = 0; i < m_trackCount; ++i )
        if ( m_tracks[i]->contains( uuid ) == true )
            return true;
    return false;
}

void
TrackHandler::stopFrameComputing()
{
    for ( unsigned int i = 0; i < m_trackCount; ++i )
        m_tracks[i]->stopFrameComputing();
}

void
TrackHandler::lengthUpdated( qint64 )
{
    qint64      maxLength = 0;

    for ( unsigned int i = 0; i < m_trackCount; ++i )
    {
        if ( m_tracks[i]->getLength() > maxLength )
            maxLength = m_tracks[i]->getLength();
    }
    if ( maxLength != m_length )
    {
        m_length = maxLength;
        emit lengthChanged( m_length );
    }
}

TrackWorkflow*
TrackHandler::track( quint32 trackId )
{
    return m_tracks[trackId];
}
