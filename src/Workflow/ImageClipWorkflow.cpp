/*****************************************************************************
 * ImageClipWorkflow.cpp : Will extract a frame from an image
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

#include "ImageClipWorkflow.h"
#include "Clip.h"
#include "ClipHelper.h"
#include "MainWorkflow.h"
#include "Media.h"
#include "VLCMediaPlayer.h"
#include "VLCMedia.h"
#include "Workflow/Types.h"

ImageClipWorkflow::ImageClipWorkflow( ClipHelper *ch ) :
        ClipWorkflow( ch ),
        m_buffer( NULL ),
        m_stackedBuffer( NULL )
{
    //This is used to queue the media player stopping, as it can't be asked for
    //from vlc's input thread (well it can but it will deadlock)
    connect( this, SIGNAL( computedFinished() ),
             this, SLOT( stopComputation() ), Qt::QueuedConnection );
}

ImageClipWorkflow::~ImageClipWorkflow()
{
    stop();
}

void
ImageClipWorkflow::initVlcOutput()
{
    char    buffer[32];

    m_vlcMedia->addOption( ":no-audio" );
    m_vlcMedia->addOption( ":no-sout-audio" );
    m_vlcMedia->addOption( ":sout=#transcode{}:smem" );
    m_vlcMedia->setVideoDataCtx( this );
    m_vlcMedia->setVideoLockCallback( reinterpret_cast<void*>( getLockCallback() ) );
    m_vlcMedia->setVideoUnlockCallback( reinterpret_cast<void*>( getUnlockCallback() ) );
    m_vlcMedia->addOption( ":sout-transcode-vcodec=RV24" );
    m_vlcMedia->addOption( ":sout-smem-time-sync" );

    sprintf( buffer, ":sout-transcode-width=%i",
             MainWorkflow::getInstance()->getWidth() );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":sout-transcode-height=%i",
             MainWorkflow::getInstance()->getHeight() );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":sout-transcode-fps=%f", (float)Clip::DefaultFPS );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":fake-duration=%d", 1000 );
    m_vlcMedia->addOption( buffer );
    sprintf( buffer, ":fake-fps=%f", m_clipHelper->clip()->getMedia()->fps() );
    m_vlcMedia->addOption( buffer );
}

void*
ImageClipWorkflow::getLockCallback() const
{
    return reinterpret_cast<void*>( &ImageClipWorkflow::lock );
}

void*
ImageClipWorkflow::getUnlockCallback() const
{
    return reinterpret_cast<void*>( &ImageClipWorkflow::unlock );
}

void*
ImageClipWorkflow::getOutput( ClipWorkflow::GetMode )
{
    QMutexLocker    lock( m_renderLock );

    return m_stackedBuffer;
}

void
ImageClipWorkflow::lock(ImageClipWorkflow *cw, void **pp_ret, int )
{
    cw->m_renderLock->lock();
    if ( cw->m_buffer == NULL )
    {
        cw->m_buffer = new Workflow::Frame( MainWorkflow::getInstance()->getWidth(),
                                            MainWorkflow::getInstance()->getHeight() );
        cw->m_stackedBuffer = new StackedBuffer( cw->m_buffer );
    }
    *pp_ret = cw->m_buffer->buffer();
}

void
ImageClipWorkflow::unlock(ImageClipWorkflow *cw, void*, int, int, int, int, qint64 )
{
    cw->m_renderLock->unlock();
    cw->emit computedFinished();
}

quint32
ImageClipWorkflow::getNbComputedBuffers() const
{
    QMutexLocker    lock( m_renderLock );

    if ( m_buffer != NULL )
        return 1;
    return 0;
}

quint32
ImageClipWorkflow::getMaxComputedBuffers() const
{
    return 1;
}

void
ImageClipWorkflow::stopComputation()
{
    m_mediaPlayer->stop();
}

void
ImageClipWorkflow::flushComputedBuffers()
{
}

ImageClipWorkflow::StackedBuffer::StackedBuffer( Workflow::Frame *frame ) :
    ::StackedBuffer<Workflow::Frame*>( frame, false )
{
}

void
ImageClipWorkflow::StackedBuffer::release()
{
    return ;
}
