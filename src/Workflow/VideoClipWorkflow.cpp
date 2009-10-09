/*****************************************************************************
 * VideoClipWorkflow.cpp : Clip workflow. Will extract a single frame from a VLCMedia
 *****************************************************************************
 * Copyright (C) 2008-2009 the VLMC team
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

#include "VideoClipWorkflow.h"

VideoClipWorkflow::VideoClipWorkflow( Clip* clip ) : ClipWorkflow( clip )
{
}

void*       VideoClipWorkflow::getLockCallback()
{
    return reinterpret_cast<void*>(&VideoClipWorkflow::lock);
}

void*       VideoClipWorkflow::getUnlockCallback()
{
    return reinterpret_cast<void*>( &VideoClipWorkflow::unlock );
}

void    VideoClipWorkflow::lock( VideoClipWorkflow* cw, void** pp_ret, int size )
{
    Q_UNUSED( size );
    cw->m_renderLock->lock();
    *pp_ret = cw->m_buffer;
//    qDebug() << '[' << (void*)cw << "] ClipWorkflow::lock";
}

void    VideoClipWorkflow::unlock( VideoClipWorkflow* cw, void* buffer, int width, int height, int bpp, int size )
{
    Q_UNUSED( buffer );
    Q_UNUSED( width );
    Q_UNUSED( height );
    Q_UNUSED( bpp );
    Q_UNUSED( size );
    cw->m_renderLock->unlock();
    cw->m_stateLock->lockForWrite();

    if ( cw->m_state == Rendering )
    {
        QMutexLocker    lock( cw->m_condMutex );

        cw->m_state = Sleeping;
        cw->m_stateLock->unlock();

        {
            QMutexLocker    lock2( cw->m_renderWaitCond->getMutex() );
            cw->m_renderWaitCond->wake();
        }
        cw->emit renderComplete( cw );
//        qDebug() << "Emmiting render completed";

//        qDebug() << "Entering cond wait";
        cw->m_waitCond->wait( cw->m_condMutex );
//        qDebug() << "Leaving condwait";
        cw->m_stateLock->lockForWrite();
        if ( cw->m_state == Sleeping )
            cw->m_state = Rendering;
        cw->m_stateLock->unlock();
    }
    else
        cw->m_stateLock->unlock();
//    qDebug() << '[' << (void*)cw << "] ClipWorkflow::unlock";
    cw->checkStateChange();
}
