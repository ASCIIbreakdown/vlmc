/*****************************************************************************
 * EffectInstanceWidget.h: Display the settings for an EffectInstance
 *****************************************************************************
 * Copyright (C) 2008-2014 VideoLAN
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

#ifndef EFFECTINSTANCEWIDGET_H
#define EFFECTINSTANCEWIDGET_H

#include <QWidget>

class   EffectInstance;
class   EffectSettingValue;

#include "EffectsEngine/Effect.h"
#include "ui_EffectInstanceWidget.h"

class   ISettingsCategoryWidget;

class EffectInstanceWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit EffectInstanceWidget( QWidget *parent = 0);
        void        setEffectInstance( EffectInstance* effectInstance );
    private:
        ISettingsCategoryWidget             *widgetFactory( EffectSettingValue *s );
        void                                clear();
    private:
        EffectInstance                      *m_effect;
        QList<ISettingsCategoryWidget*>     m_settings;
        QList<QWidget*>                     m_widgets;
        Ui::EffectSettingWidget             *m_ui;

    public slots:
        void                                save();
    };

#endif // EFFECTINSTANCEWIDGET_H
