/*****************************************************************************
 * VideoClipWorkflow.cpp : Clip workflow. Will extract a single frame from a VLCMedia
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

#include "Clip.h"
#include "LightVideoFrame.h"
#include "MainWorkflow.h"
#include "StackedBuffer.hpp"
#include "VideoClipWorkflow.h"
#include "VLCMedia.h"
#include "WaitCondition.hpp"

#include <QReadWriteLock>

VideoClipWorkflow::VideoClipWorkflow( ClipHelper *ch ) :
        ClipWorkflow( ch ),
        m_width( 0 ),
        m_height( 0 )
{
}

VideoClipWorkflow::~VideoClipWorkflow()
{
    releasePrealocated();
}

void
VideoClipWorkflow::releasePrealocated()
{
    while ( m_availableBuffers.isEmpty() == false )
        delete m_availableBuffers.dequeue();
    while ( m_computedBuffers.isEmpty() == false )
        delete m_computedBuffers.dequeue();
}

void
VideoClipWorkflow::preallocate()
{
    quint32     newWidth = MainWorkflow::getInstance()->getWidth();
    quint32     newHeight = MainWorkflow::getInstance()->getHeight();
    if ( newWidth != m_width || newHeight != m_height )
    {
        m_width = newWidth;
        m_height = newHeight;
        while ( m_availableBuffers.isEmpty() == false )
            delete m_availableBuffers.dequeue();
        for ( unsigned int i = 0; i < VideoClipWorkflow::nbBuffers; ++i )
        {
            m_availableBuffers.enqueue( new LightVideoFrame( newWidth, newHeight ) );
        }
    }
}

void
VideoClipWorkflow::initVlcOutput()
{
    char        buffer[32];

    preallocate();
    m_vlcMedia->addOption( ":no-audio" );
    m_vlcMedia->addOption( ":no-sout-audio" );
    m_vlcMedia->addOption( ":sout=#transcode{}:smem" );
    m_vlcMedia->setVideoDataCtx( this );
    m_vlcMedia->setVideoLockCallback( reinterpret_cast<void*>( getLockCallback() ) );
    m_vlcMedia->setVideoUnlockCallback( reinterpret_cast<void*>( getUnlockCallback() ) );
    m_vlcMedia->addOption( ":sout-transcode-vcodec=RV24" );
    if ( m_fullSpeedRender == false )
        m_vlcMedia->addOption( ":sout-smem-time-sync" );
    else
        m_vlcMedia->addOption( ":no-sout-smem-time-sync" );

    sprintf( buffer, ":sout-transcode-width=%i", m_width );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":sout-transcode-height=%i", m_height );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":sout-transcode-fps=%f", (float)Clip::DefaultFPS );
    m_vlcMedia->addOption( buffer );
}

void*
VideoClipWorkflow::getLockCallback() const
{
    return reinterpret_cast<void*>(&VideoClipWorkflow::lock);
}

void*
VideoClipWorkflow::getUnlockCallback() const
{
    return reinterpret_cast<void*>( &VideoClipWorkflow::unlock );
}

void*
VideoClipWorkflow::getOutput( ClipWorkflow::GetMode mode )
{
    QMutexLocker    lock( m_renderLock );

    if ( shouldRender() == false )
        return NULL;
    if ( getNbComputedBuffers() == 0 )
        m_renderWaitCond->wait( m_renderLock );
    //Recheck again, as the WaitCondition may have been awaken when stopping.
    if ( getNbComputedBuffers() == 0 )
        return NULL;
    ::StackedBuffer<LightVideoFrame*>* buff;
    if ( mode == ClipWorkflow::Pop )
        buff = new StackedBuffer( m_computedBuffers.dequeue(), this, true );
    else if ( mode == ClipWorkflow::Get )
        buff = new StackedBuffer( m_computedBuffers.head(), NULL, false );
    postGetOutput();
    return buff;
}

void
VideoClipWorkflow::lock( VideoClipWorkflow *cw, void **pp_ret, int size )
{
    Q_UNUSED( size );
    LightVideoFrame*    lvf = NULL;

    cw->m_renderLock->lock();
    if ( cw->m_availableBuffers.isEmpty() == true )
    {
        lvf = new LightVideoFrame( cw->m_width, cw->m_height );
    }
    else
        lvf = cw->m_availableBuffers.dequeue();
    cw->m_computedBuffers.enqueue( lvf );
    *pp_ret = (*(lvf))->frame.octets;
}

void
VideoClipWorkflow::unlock( VideoClipWorkflow *cw, void *buffer, int width,
                           int height, int bpp, int size, qint64 pts )
{
    Q_UNUSED( buffer );
    Q_UNUSED( width );
    Q_UNUSED( height );
    Q_UNUSED( bpp );
    Q_UNUSED( size );

    cw->computePtsDiff( pts );
    LightVideoFrame     *lvf = cw->m_computedBuffers.last();
    (*(lvf))->ptsDiff = cw->m_currentPts - cw->m_previousPts;
    cw->commonUnlock();
    cw->m_renderWaitCond->wakeAll();
    cw->m_renderLock->unlock();
}

uint32_t
VideoClipWorkflow::getNbComputedBuffers() const
{
    return m_computedBuffers.count();
}

uint32_t
VideoClipWorkflow::getMaxComputedBuffers() const
{
    return VideoClipWorkflow::nbBuffers;
}

void
VideoClipWorkflow::releaseBuffer( LightVideoFrame *lvf )
{
    QMutexLocker    lock( m_renderLock );

    m_availableBuffers.enqueue( lvf );
}

void
VideoClipWorkflow::flushComputedBuffers()
{
    QMutexLocker    lock( m_renderLock );

    while ( m_computedBuffers.isEmpty() == false )
        m_availableBuffers.enqueue( m_computedBuffers.dequeue() );
}

VideoClipWorkflow::StackedBuffer::StackedBuffer( LightVideoFrame *lvf,
                                                    VideoClipWorkflow *poolHandler,
                                                    bool mustBeReleased) :
    ::StackedBuffer<LightVideoFrame*>( lvf, mustBeReleased ),
    m_poolHandler( poolHandler )
{
}

void
VideoClipWorkflow::StackedBuffer::release()
{
    if ( m_mustRelease == true && m_poolHandler.isNull() == false )
        m_poolHandler->releaseBuffer( m_buff );
    delete this;
}
