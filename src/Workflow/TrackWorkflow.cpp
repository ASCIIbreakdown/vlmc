/*****************************************************************************
 * TrackWorkflow.cpp : Will query the Clip workflow for each successive clip in the track
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Yikei Lu    <luyikei.qmltu@gmail.com>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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


#include "TrackWorkflow.h"

#include "Project/Project.h"
#include "Media/Clip.h"
#include "EffectsEngine/EffectHelper.h"
#include "Backend/VLC/VLCSource.h"
#include "Backend/MLT/MLTTrack.h"
#include "Backend/MLT/MLTTractor.h"
#include "Main/Core.h"
#include "Library/Library.h"
#include "MainWorkflow.h"
#include "Media/Media.h"
#include "Types.h"
#include "vlmc.h"
#include "Tools/VlmcDebug.h"


#include <QReadLocker>
#include <QMutex>

TrackWorkflow::TrackWorkflow( quint32 trackId, Backend::ITractor* tractor ) :
        m_trackId( trackId )
{
    m_clipsLock = new QReadWriteLock;

    auto audioTrack = new Backend::MLT::MLTTrack;
    audioTrack->setVideoOutput( false );
    m_audioTrack = audioTrack;

    auto videoTrack = new Backend::MLT::MLTTrack;
    videoTrack->setAudioOutput( false );
    m_videoTrack = videoTrack;

    m_tractor = new Backend::MLT::MLTTractor;
    m_tractor->setTrack( *m_videoTrack, 0 );
    m_tractor->setTrack( *m_audioTrack, 1 );

    tractor->setTrack( *m_tractor, trackId );

    connect( this, SIGNAL( effectAdded( EffectHelper*, qint64 ) ),
             this, SLOT( __effectAdded( EffectHelper*, qint64) ) );
    connect( this, SIGNAL( effectMoved( EffectHelper*, qint64 ) ),
             this, SLOT( __effectMoved( EffectHelper*, qint64 ) ) );
    connect( this, SIGNAL( effectRemoved( QUuid ) ),
             this, SLOT( __effectRemoved(QUuid ) ) );
}

TrackWorkflow::~TrackWorkflow()
{
    delete m_audioTrack;
    delete m_videoTrack;
    delete m_tractor;
    delete m_clipsLock;
}

void
TrackWorkflow::addClip( Clip* clip, qint64 start )
{
    if ( clip->formats().testFlag( Clip::Audio ) )
        m_audioTrack->insertAt( *clip->producer(), start );
    else if ( clip->formats().testFlag( Clip::Video ) )
        m_videoTrack->insertAt( *clip->producer(), start );
    m_clips.insertMulti( start, clip );
    emit clipAdded( this, clip, start );
}

qint64
TrackWorkflow::getClipPosition( const QUuid& uuid ) const
{
    auto     it = m_clips.begin();
    auto     end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == uuid )
            return it.key();
        ++it;
    }
    return -1;
}

Clip*
TrackWorkflow::clip( const QUuid& uuid )
{
    auto     it = m_clips.begin();
    auto     end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == uuid )
            return it.value();
        ++it;
    }
    return nullptr;
}

void
TrackWorkflow::moveClip( const QUuid& id, qint64 startingFrame )
{
    QWriteLocker    lock( m_clipsLock );

    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == id )
        {
            auto clip = it.value();
            auto track = ( clip->formats().testFlag( Clip::Audio ) ) ? m_audioTrack : m_videoTrack;
            auto producer = track->clipAt( it.key() );
            track->remove( track->clipIndexAt( it.key() ) );
            track->insertAt( *producer, startingFrame );
            delete producer;

            m_clips.erase( it );
            m_clips.insertMulti( startingFrame, clip );
            emit clipMoved( this, clip->uuid(), startingFrame );
            return ;
        }
        ++it;
    }
}

void
TrackWorkflow::clipDestroyed( const QUuid& id )
{
    QWriteLocker    lock( m_clipsLock );

    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == id )
        {
            auto   clip = it.value();
            m_clips.erase( it );
            clip->disconnect( this );
            emit clipRemoved( this, id );
            return ;
        }
        ++it;
    }
}

Clip*
TrackWorkflow::removeClip( const QUuid& id )
{
    QWriteLocker    lock( m_clipsLock );

    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == id )
        {
            auto    clip = it.value();
            auto    track = ( clip->formats().testFlag( Clip::Audio ) ) ? m_audioTrack : m_videoTrack;
            track->remove( track->clipIndexAt( it.key() ) );
            m_clips.erase( it );
            clip->disconnect( this );
            emit clipRemoved( this, clip->uuid() );
            return clip;
        }
        ++it;
    }
    return nullptr;
}

QVariant
TrackWorkflow::toVariant() const
{
    /*
    QVariantList l;
    for ( auto it = m_clips.cbegin(); it != m_clips.cend(); it++ )
    {
        auto    clip = it.value();
        l << QVariantHash{
                    { "clip", clip->uuid() },
                    { "begin", clip->begin() },
                    { "end", clip->end() },
                    { "startFrame", it.key() },
                    { "filters", clip->toVariant() }
                };
    }
    QVariantHash h{ { "clips", l } };
    return QVariant( h );*/
    return QVariant();
}

void
TrackWorkflow::loadFromVariant( const QVariant &variant )
{
    for ( auto& var : variant.toMap()[ "clips" ].toList() )
    {
        QVariantMap m = var.toMap();
        const QString& uuid     = m["clip"].toString();
        qint64 startFrame       = m["startFrame"].toLongLong();
        qint64 begin            = m["begin"].toLongLong();
        qint64 end              = m["end"].toLongLong();

        if ( uuid.isEmpty() )
        {
            vlmcWarning() << "Invalid clip node";
            return ;
        }

        Clip  *clip = Core::instance()->workflow()->createClip( QUuid( uuid ) );
        if ( clip == nullptr )
            continue ;
        clip->setBoundaries( begin, end );
        addClip( clip, startFrame );

        // TODO clip->clipWorkflow()->loadFromVariant( m["filters"] );
    }
}

void
TrackWorkflow::clear()
{
    QWriteLocker    lock( m_clipsLock );
    m_clips.clear();
}

void
TrackWorkflow::muteClip( const QUuid &uuid )
{
    /* TODO
    QWriteLocker    lock( m_clipsLock );

    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == uuid )
        {
            it.value()->mute();
            return ;
        }
        ++it;
    }
    vlmcWarning() << "Failed to mute clip" << uuid << "it probably doesn't exist "
            "in this track";
            */
}

void
TrackWorkflow::unmuteClip( const QUuid &uuid )
{
    /* TODO
    QWriteLocker    lock( m_clipsLock );

    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == uuid )
        {
            it.value()->unmute();
            return ;
        }
        ++it;
    }
    vlmcWarning() << "Failed to unmute clip" << uuid << "it probably doesn't exist "
            "in this track";
            */
}

bool
TrackWorkflow::contains( const QUuid &uuid ) const
{
    auto       it = m_clips.begin();
    auto       end = m_clips.end();

    while ( it != end )
    {
        if ( it.value()->uuid() == uuid ||
             it.value()->isChild( uuid ) )
            return true;
        ++it;
    }
    return false;
}

quint32
TrackWorkflow::trackId() const
{
    return m_trackId;
}

void
TrackWorkflow::__effectAdded( EffectHelper* helper, qint64 pos )
{
    /* TODO
    if ( helper->target()->effectType() == ClipEffectUser )
    {
        ClipWorkflow    *cw = qobject_cast<ClipWorkflow*>( helper->target() );
        Q_ASSERT( cw != nullptr );
        pos += getClipPosition( cw->clip()->uuid() );
    }
    emit effectAdded( this, helper, pos );
    */
}

void
TrackWorkflow::__effectRemoved( const QUuid& uuid )
{
    emit effectRemoved( this, uuid );
}

void
TrackWorkflow::__effectMoved( EffectHelper* helper, qint64 pos )
{
    /* TODO
    if ( helper->target()->effectType() == ClipEffectUser )
    {
        ClipWorkflow    *cw = qobject_cast<ClipWorkflow*>( helper->target() );
        Q_ASSERT( cw != nullptr );
        pos += getClipPosition( cw->clip()->uuid() );
    }
    emit effectMoved( this, helper->uuid(), pos );
    */
}

Backend::IProducer*
TrackWorkflow::producer()
{
    return m_tractor;
}
