/*****************************************************************************
 * MemoryPool.hpp: Generic memory pool, that will reinstantiate
 * a new object each time
 *****************************************************************************
 * Copyright (C) 2008-2014 VideoLAN
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

#ifndef MEMORYPOOL_HPP
#define MEMORYPOOL_HPP

#include <QMutex>
#include <QQueue>

#include "Singleton.hpp"
#include "VlmcDebug.h"

template <typename T, size_t NB_ELEM = 5>
class       MemoryPool : public Singleton< MemoryPool<T, NB_ELEM> >
{
public:
    T*      get()
    {
        QMutexLocker    lock( m_mutex );
        if ( m_pool.size() == 0 )
        {
            vlmcCritical() << "Pool is empty !!";
            return new T;
        }
        quint8*    ptr = m_pool.dequeue();
        T*  ret = new (ptr) T;
        return ret;
    }
    void    release( T* toRelease )
    {
        QMutexLocker    lock( m_mutex );
        toRelease->~T();
        m_pool.enqueue( reinterpret_cast<quint8*>( toRelease ) );
    }
private:
    MemoryPool()
    {
        for ( size_t i = 0; i < NB_ELEM; ++i )
            m_pool.enqueue( new quint8[ sizeof(T) ] );
        m_mutex = new QMutex;
    }
    ~MemoryPool()
    {
        while ( m_pool.size() != 0 )
        {
            quint8*  ptr = m_pool.dequeue();
            delete ptr;
        }
        delete m_mutex;
    }
    QQueue<quint8*>     m_pool;
    QMutex*             m_mutex;
    friend class Singleton< MemoryPool<T> >;
};

#endif // MEMORYPOOL_HPP

