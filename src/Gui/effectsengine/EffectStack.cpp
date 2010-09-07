/*****************************************************************************
 * EffectStack.cpp: Represent an effect stack, and allow parameters editing.
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

#include "EffectStack.h"
#include "ui_EffectStack.h"

#include "EffectHelper.h"
#include "EffectInstance.h"
#include "EffectInstanceWidget.h"
#include "EffectUser.h"
#include "EffectInstanceListModel.h"

#include <QStackedLayout>

EffectStack::EffectStack( EffectUser *user, QWidget *parent ):
    QDialog( parent ),
    m_ui( new Ui::EffectStack ),
    m_user( user )
{
    m_ui->setupUi( this );

    m_model = new EffectInstanceListModel( user );
    m_ui->list->setModel( m_model );
    connect( m_ui->list, SIGNAL( clicked( QModelIndex ) ),
             this, SLOT( selectedChanged( QModelIndex ) ) );
    connect( m_ui->upButton, SIGNAL( clicked() ), this, SLOT( moveUp() ) );
    connect( m_ui->downButton, SIGNAL( clicked() ), this, SLOT( moveDown() ) );
    connect( m_ui->removeButton, SIGNAL( clicked() ), this, SLOT( remove() ) );
    connect( m_ui->addButton, SIGNAL( clicked() ), this, SLOT( add() ) );

    m_ui->addComboBox->addItems( EffectsEngine::getInstance()->effects( Effect::Filter ) );
    m_stackedLayout = new QStackedLayout;
    m_ui->horizontalLayout->addLayout( m_stackedLayout );
    //Add an empty instance widget.
    m_stackedLayout->addWidget( new EffectInstanceWidget( this ) );
    //Create instance widgets for already inserted effects
    foreach ( EffectHelper *helper, user->effects( Effect::Filter ) )
        addInstanceWidget( helper->effectInstance() );
}

EffectStack::~EffectStack()
{
    delete m_model;
    delete m_ui;
}

void
EffectStack::addInstanceWidget( EffectInstance *instance )
{
    EffectInstanceWidget    *w = new EffectInstanceWidget( this );
    w->setEffectInstance( instance );
    m_stackedLayout->addWidget( w );
    m_instanceWidgets[instance->effect()->name()] = w;
}

void
EffectStack::selectedChanged( const QModelIndex &index )
{
    EffectInstance  *inst = m_model->data( index, Qt::EditRole ).value<EffectHelper*>()->effectInstance();
    m_stackedLayout->setCurrentWidget( m_instanceWidgets[inst->effect()->name()] );
}

void
EffectStack::moveUp()
{
    m_model->moveUp( m_ui->list->currentIndex() );
    if ( m_ui->list->currentIndex().row() > 0 )
        m_ui->list->setCurrentIndex( m_ui->list->currentIndex().sibling( m_ui->list->currentIndex().row() - 1, 0 ) );
}

void
EffectStack::moveDown()
{
    m_model->moveDown( m_ui->list->currentIndex() );
    if ( m_ui->list->currentIndex().row() < m_model->rowCount( QModelIndex() ) - 1 )
        m_ui->list->setCurrentIndex( m_ui->list->currentIndex().sibling( m_ui->list->currentIndex().row() + 1, 0 ) );
}

void
EffectStack::remove()
{
    m_model->removeRow( m_ui->list->currentIndex().row() );
    if ( m_ui->list->currentIndex().isValid() == true )
        selectedChanged( m_ui->list->currentIndex() );
    else
        m_stackedLayout->setCurrentIndex( 0 );
}

void
EffectStack::add()
{
    EffectHelper    *helper = m_model->add( m_ui->addComboBox->currentText() );
    addInstanceWidget( helper->effectInstance() );
}
