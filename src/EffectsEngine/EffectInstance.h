/*****************************************************************************
 * EffectInstance.h: Handle an effect instance.
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
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

#ifndef EFFECTINSTANCE_H
#define EFFECTINSTANCE_H

class   Effect;

#include <QtGlobal>
#include "frei0r/frei0r.h"

class EffectInstance
{
    public:
        void            init( quint32 width, quint32 height );
        Effect*         effect();

    protected:
        EffectInstance( Effect *effect );
        ~EffectInstance();

        Effect          *m_effect;
        quint32         m_width;
        quint32         m_height;
        f0r_instance_t  m_instance;

        friend class    Effect;

};

#endif // EFFECTINSTANCE_H
