/*****************************************************************************
 * ClipWorkflow.cpp : Clip workflow. Will extract a single frame from a VLCMedia
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <hugo@vlmc.org>
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
#include "Clip.h"
#include "ClipHelper.h"
#include "ClipWorkflow.h"
#include "Media.h"
#include "MemoryPool.hpp"
#include "WaitCondition.hpp"
#include "Workflow/Types.h"
#include "VLCMedia.h"
#include "VLCMediaPlayer.h"

#include <QReadWriteLock>
#include <QWaitCondition>
#include <QtDebug>

ClipWorkflow::ClipWorkflow( ClipHelper* ch ) :
                m_mediaPlayer(NULL),
                m_clipHelper( ch ),
                m_state( ClipWorkflow::Stopped )
{
    connect( this, SIGNAL( error() ), ch, SIGNAL( error() ) );
    m_stateLock = new QReadWriteLock;
    m_initWaitCond = new WaitCondition;
    m_renderLock = new QMutex;
    m_renderWaitCond = new QWaitCondition;
}

ClipWorkflow::~ClipWorkflow()
{
    //Don't call stop() method from here, the Vtable is probably already gone.
    delete m_renderWaitCond;
    delete m_renderLock;
    delete m_initWaitCond;
    delete m_stateLock;
}

void
ClipWorkflow::initialize()
{
    setState( ClipWorkflow::Initializing );

    m_vlcMedia = new LibVLCpp::Media( m_clipHelper->clip()->getMedia()->mrl() );
    m_currentPts = -1;
    m_previousPts = -1;
    m_pauseDuration = -1;
    initVlcOutput();
    m_mediaPlayer = MemoryPool<LibVLCpp::MediaPlayer>::getInstance()->get();
    m_mediaPlayer->setMedia( m_vlcMedia );

    connect( m_mediaPlayer, SIGNAL( playing() ), this, SLOT( loadingComplete() ), Qt::DirectConnection );
    connect( m_mediaPlayer, SIGNAL( endReached() ), this, SLOT( clipEndReached() ), Qt::DirectConnection );
    connect( m_mediaPlayer, SIGNAL( errorEncountered() ), this, SLOT( errorEncountered() ) );
    m_mediaPlayer->play();
}

void
ClipWorkflow::loadingComplete()
{
    adjustBegin();
    disconnect( m_mediaPlayer, SIGNAL( playing() ), this, SLOT( loadingComplete() ) );
    connect( m_mediaPlayer, SIGNAL( playing() ), this, SLOT( mediaPlayerUnpaused() ), Qt::DirectConnection );
    connect( m_mediaPlayer, SIGNAL( paused() ), this, SLOT( mediaPlayerPaused() ), Qt::DirectConnection );
    QMutexLocker    lock( m_initWaitCond->getMutex() );
    setState( Rendering );
    m_initWaitCond->wake();
}

void
ClipWorkflow::adjustBegin()
{
    if ( m_clipHelper->clip()->getMedia()->fileType() == Media::Video ||
         m_clipHelper->clip()->getMedia()->fileType() == Media::Audio )
    {
        m_mediaPlayer->setTime( m_clipHelper->begin() /
                                m_clipHelper->clip()->getMedia()->fps() * 1000 );
    }
}

ClipWorkflow::State
ClipWorkflow::getState() const
{
    return m_state;
}

void
ClipWorkflow::clipEndReached()
{
    setState( EndReached );
}

void
ClipWorkflow::stop()
{
    stopRenderer();
    flushComputedBuffers();
    releasePrealocated();
}

void
ClipWorkflow::stopRenderer()
{
    QWriteLocker    lockState( m_stateLock );

    //Let's make sure the ClipWorkflow isn't beeing stopped from another thread.
    if ( m_mediaPlayer &&
         m_state != Stopping && m_state != Stopped )
    {
        //Set a specific state to avoid operation (getOutput beeing called actually, as it
        //would freeze the render waiting for a frame) while the stopping process occurs.
        m_state = Stopping;
        {
            QMutexLocker    lock( m_initWaitCond->getMutex() );
            m_initWaitCond->wake();
        }
        {
            QMutexLocker    lock( m_renderLock );
            m_renderWaitCond->wakeAll();
        }
        m_mediaPlayer->stop();
        m_mediaPlayer->disconnect();
        MemoryPool<LibVLCpp::MediaPlayer>::getInstance()->release( m_mediaPlayer );
        m_mediaPlayer = NULL;
        delete m_vlcMedia;
        m_state = Stopped;
    }
}

void
ClipWorkflow::setTime( qint64 time )
{
    m_mediaPlayer->setTime( time );
    resyncClipWorkflow();
    QWriteLocker    lock( m_stateLock );
    if ( m_state == ClipWorkflow::Paused )
    {
        m_mediaPlayer->pause();
        m_state = ClipWorkflow::UnpauseRequired;
    }
}

void            ClipWorkflow::setState( State state )
{
    QWriteLocker    lock( m_stateLock );
    m_state = state;
}

QReadWriteLock* ClipWorkflow::getStateLock()
{
    return m_stateLock;
}

bool
ClipWorkflow::waitForCompleteInit()
{
    m_stateLock->lockForRead();
    if ( m_state != ClipWorkflow::Rendering && m_state != ClipWorkflow::Error )
    {
        if ( m_state == ClipWorkflow::Error )
        {
            m_stateLock->unlock();
            return false;
        }
        m_stateLock->unlock();
        QMutexLocker    lock( m_initWaitCond->getMutex() );
        m_initWaitCond->waitLocked();

        m_stateLock->lockForRead();
        if ( m_state == ClipWorkflow::Error )
        {
            m_stateLock->unlock();
            return false;
        }
    }
    m_stateLock->unlock();
    return true;
}

LibVLCpp::MediaPlayer*       ClipWorkflow::getMediaPlayer()
{
    return m_mediaPlayer;
}

void        ClipWorkflow::postGetOutput()
{
    //If we're running out of computed buffers, refill our stack.
    if ( getNbComputedBuffers() < getMaxComputedBuffers() / 3 )
    {
        QWriteLocker        lock( m_stateLock );
        if ( m_state == ClipWorkflow::Paused )
        {
            m_state = ClipWorkflow::UnpauseRequired;
//            This will act like an "unpause";
            m_mediaPlayer->pause();
        }
    }
}

void        ClipWorkflow::commonUnlock()
{
    //Don't test using availableBuffer, as it may evolve if a buffer is required while
    //no one is available : we would spawn a new buffer, thus modifying the number of available buffers
    if ( getNbComputedBuffers() >= getMaxComputedBuffers() )
    {
        setState( ClipWorkflow::PauseRequired );
        m_mediaPlayer->pause();
    }
}

void    ClipWorkflow::computePtsDiff( qint64 pts )
{
    if ( m_previousPts == -1 )
        m_previousPts = pts;
    if ( m_currentPts == -1 )
        m_currentPts = pts;
    if ( m_pauseDuration != -1 )
    {
        m_previousPts = m_currentPts + m_pauseDuration;
        m_pauseDuration = -1;
    }
    else
        m_previousPts = m_currentPts;
    m_currentPts = qMax( pts, m_previousPts );
}

void    ClipWorkflow::mediaPlayerPaused()
{
    setState( ClipWorkflow::Paused );
    m_beginPausePts = mdate();
}

void    ClipWorkflow::mediaPlayerUnpaused()
{
    setState( ClipWorkflow::Rendering );
    m_pauseDuration = mdate() - m_beginPausePts;
}

void    ClipWorkflow::resyncClipWorkflow()
{
    flushComputedBuffers();
    m_previousPts = -1;
    m_currentPts = -1;
}

void
ClipWorkflow::setFullSpeedRender( bool val )
{
    m_fullSpeedRender = val;
}

void
ClipWorkflow::mute()
{
    stop();
    setState( Muted );
}

void
ClipWorkflow::unmute()
{
    setState( Stopped );
}

void
ClipWorkflow::requireResync()
{
    m_resyncRequired = 1;
}

bool
ClipWorkflow::isResyncRequired()
{
    if ( m_resyncRequired == 1 )
    {
        m_resyncRequired = 0;
        return true;
    }
    return false;
}

void
ClipWorkflow::errorEncountered()
{
    stopRenderer();
    setState( Error );
    emit error();
}

bool
ClipWorkflow::shouldRender() const
{
    QReadLocker lock( m_stateLock );
    return ( m_state != ClipWorkflow::Error &&
             m_state != ClipWorkflow::Stopped &&
             m_state != ClipWorkflow::Stopping );
}

void
ClipWorkflow::save( QXmlStreamWriter &project ) const
{
    project.writeAttribute( "uuid", m_clipHelper->clip()->fullId() );
    project.writeAttribute( "begin", QString::number( m_clipHelper->begin() ) );
    project.writeAttribute( "end", QString::number( m_clipHelper->end() ) );
    project.writeAttribute( "helper", m_clipHelper->uuid().toString() );
}
