/*****************************************************************************
 * MainWorkflow.cpp : Will query all of the track workflows to render the final
 *                    image
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


#include "vlmc.h"
#include "Backend/MLT/MLTTractor.h"
#include "Backend/MLT/MLTTrack.h"
#include "Renderer/AbstractRenderer.h"
#include "Project/Project.h"
#include "Media/Clip.h"
#include "Library/Library.h"
#include "MainWorkflow.h"
#include "Project/Project.h"
#include "TrackWorkflow.h"
#include "Settings/Settings.h"
#include "Tools/VlmcDebug.h"
#include "Tools/RendererEventWatcher.h"
#include "Workflow/Types.h"

#include <QMutex>

MainWorkflow::MainWorkflow( Settings* projectSettings, int trackCount ) :
        m_mediaContainer( new MediaContainer ),
        m_trackCount( trackCount ),
        m_settings( new Settings ),
        m_renderer( new AbstractRenderer ),
        m_tractor( new Backend::MLT::MLTTractor )
{
    m_renderer->setProducer( m_tractor );

    connect( m_renderer->eventWatcher(), &RendererEventWatcher::lengthChanged, this, &MainWorkflow::lengthChanged );
    connect( m_renderer->eventWatcher(), &RendererEventWatcher::endReached, this, &MainWorkflow::mainWorkflowEndReached );

    for ( int i = 0; i < trackCount; ++i )
    {
        Toggleable<TrackWorkflow*> track;
        m_tracks << track;
        m_tracks[i].setPtr( new TrackWorkflow( i, m_tractor ) );
    }

    m_settings->createVar( SettingValue::List, "tracks", QVariantList(), "", "", SettingValue::Nothing );
    connect( m_settings, &Settings::postLoad, this, &MainWorkflow::postLoad, Qt::DirectConnection );
    connect( m_settings, &Settings::preSave, this, &MainWorkflow::preSave, Qt::DirectConnection );
    projectSettings->addSettings( "Workspace", *m_settings );
}

MainWorkflow::~MainWorkflow()
{
    for ( auto track : m_tracks )
        delete track;
    m_tracks.clear();
    delete m_tractor;
    delete m_renderer;
    delete m_settings;
    delete m_mediaContainer;
}

qint64
MainWorkflow::getClipPosition( const QUuid& uuid, unsigned int trackId ) const
{
    Q_ASSERT( trackId < m_trackCount );
    return m_tracks[trackId]->getClipPosition( uuid );
}

void
MainWorkflow::muteTrack( unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );
    m_tracks[trackId].deactivate();
}

void
MainWorkflow::unmuteTrack( unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );
    m_tracks[trackId].activate();
}

void
MainWorkflow::muteClip( const QUuid& uuid, unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );
    m_tracks[trackId]->muteClip( uuid );
}

void
MainWorkflow::unmuteClip( const QUuid& uuid, unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );
    m_tracks[trackId]->unmuteClip( uuid );
}

Clip*
MainWorkflow::clip( const QUuid &uuid, unsigned int trackId )
{
    Q_ASSERT( trackId < m_trackCount );
    return m_tracks[trackId]->clip( uuid );
}

void
MainWorkflow::clear()
{
    for ( auto track : m_tracks )
        track->clear();
    emit cleared();
}

AbstractRenderer*
MainWorkflow::renderer()
{
    return m_renderer;
}

int
MainWorkflow::getTrackCount() const
{
    return m_trackCount;
}

bool
MainWorkflow::contains( const QUuid &uuid ) const
{
    for ( auto track : m_tracks )
        if ( track->contains( uuid ) == true )
            return true;
    return false;
}

quint32
MainWorkflow::trackCount() const
{
    return m_trackCount;
}

Clip*
MainWorkflow::createClip( const QUuid& uuid )
{
    Clip* clip = Core::instance()->library()->clip( uuid );
    if ( clip == nullptr )
    {
        vlmcCritical() << "Couldn't find an acceptable parent to be added.";
        return nullptr;
    }
    auto newClip = new Clip( clip );
    m_mediaContainer->addClip( newClip );
    return newClip;
}

void
MainWorkflow::deleteClip( const QUuid& uuid )
{
    m_mediaContainer->deleteClip( uuid );
}

void
MainWorkflow::preSave()
{
    QVariantList l;
    for ( auto track : m_tracks )
        l << track->toVariant();
    m_settings->value( "tracks" )->set( l );
}

void
MainWorkflow::postLoad()
{
    QVariantList l = m_settings->value( "tracks" )->get().toList();
    for ( int i = 0; i < l.size(); ++i )
        m_tracks[i]->loadFromVariant( l[i] );
}

TrackWorkflow*
MainWorkflow::track( quint32 trackId )
{
    return m_tracks[ trackId ];
}
