/*****************************************************************************
 * PreferenceWidget.cpp: Abstract class that will be used to save load / preferences
 * values.
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Clement CHAVANCE <kinder@vlmc.org>
 *          Hugo Beauzée-Luyssen <hugo@vlmc.org>
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

#include "ISettingsCategorieWidget.h"
#include "PreferenceWidget.h"
#include "SettingsManager.h"

#include "BoolWidget.h"
#include "DoubleWidget.h"
#include "IntWidget.h"
#include "KeyboardShortcut.h"
#include "LanguageWidget.h"
#include "StringWidget.h"

#include <QFormLayout>
#include <QtDebug>
#include <QWidget>
#include <QLabel>

PreferenceWidget::PreferenceWidget( const QString &categorie, SettingsManager::Type type,
                                    QWidget *parent ) :
    QScrollArea( parent ),
    m_categorie( categorie )
{
    QWidget     *container = new QWidget( this );
    SettingsManager::SettingHash    settings =
            SettingsManager::getInstance()->group( categorie, type );
    QFormLayout *layout = new QFormLayout( container );
    layout->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
    foreach ( SettingValue* s, settings.values() )
    {
        ISettingsCategorieWidget    *widget = widgetFactory( s );
        QLabel                      *label = new QLabel( s->name(), this );
        label->setToolTip( s->description() );
        widget->widget()->setToolTip( s->description() );
        layout->addRow( label, widget->widget() );
        m_settings.push_back( widget );
    }
    setWidget( container );
    setWidgetResizable( true );
    setFrameStyle( QFrame::NoFrame );
    m_categorie[0] = m_categorie[0].toUpper();
}

ISettingsCategorieWidget*
PreferenceWidget::widgetFactory( SettingValue *s )
{
    switch ( s->type() )
    {
    case SettingValue::KeyboardShortcut:
        return new KeyboardShortcut( s, this );
    case SettingValue::Language:
        return new LanguageWidget( s, this );
    case SettingValue::String:
        return new StringWidget( s, this );
    case SettingValue::Int:
        return new IntWidget( s, this );
    case SettingValue::Double:
        return new DoubleWidget( s, this );
    case SettingValue::Bool:
        return new BoolWidget( s, this );
    }
}

void
PreferenceWidget::save()
{
    foreach ( ISettingsCategorieWidget* w, m_settings )
        w->save();
}

const QString&
PreferenceWidget::categorie() const
{
    return m_categorie;
}
