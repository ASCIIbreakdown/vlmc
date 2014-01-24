/*****************************************************************************
 * MediaCellView.cpp
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Thomas Boquet <thomas.boquet@gmail.com>
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

#include "MediaCellView.h"
#include "ui_MediaCellView.h"

#include "Clip.h"
#include "ClipProperty.h"
#include "Library.h"
#include "Media.h"
#include "MetaDataManager.h"
#include "MainWorkflow.h"
#include "Workspace.h"

#include <QMenu>
#include <QMessageBox>
#include <QTime>

MediaCellView::MediaCellView( Clip* clip, QWidget *parent ) :
        QWidget( parent ),
        m_ui( new Ui::MediaCellView ),
        m_clip( clip )
{
    m_ui->setupUi( this );
    setFocusPolicy( Qt::ClickFocus );
    setAutoFillBackground( true );
    connect( m_ui->delLabel, SIGNAL( clicked( QWidget*, QMouseEvent* ) ),
             this, SLOT( deleteButtonClicked( QWidget*, QMouseEvent* ) ) );
    connect( m_ui->arrow,
             SIGNAL( clicked( QWidget*, QMouseEvent* ) ),
             SLOT( arrowButtonClicked( QWidget*, QMouseEvent* ) ) );
    m_ui->clipCount->setText( QString::number( clip->getChilds()->count() ) );
    connect( clip->getChilds(), SIGNAL( newClipLoaded( Clip* ) ),
             this, SLOT( nbClipUpdated() ) );
    connect( clip->getChilds(), SIGNAL( clipRemoved( const QUuid& ) ),
             this, SLOT( nbClipUpdated() ) );
    if ( clip->getChilds()->count() == 0 )
    {
        m_ui->clipCount->hide();
        m_ui->clipCountLabel->hide();
        m_ui->arrow->hide();
    }
    if ( clip->getMedia()->isMetadataComputed() == false )
    {
        m_ui->thumbnail->setEnabled( false );
        connect( MetaDataManager::getInstance(), SIGNAL( startingComputing( const Media* )),
                 this, SLOT( metadataComputingStarted( const Media* ) ), Qt::DirectConnection );
    }
    connect( clip->getMedia(), SIGNAL( metaDataComputed(const Media*) ),
             this, SLOT( metadataUpdated( const Media*) ) );
    connect( clip->getMedia(), SIGNAL( snapshotComputed(const Media*) ),
             this, SLOT( snapshotUpdated(const Media*) ) );

    setThumbnail( clip->getMedia()->snapshot() );
    setTitle( clip->getMedia()->fileName() );
    setLength( clip->lengthSecond() * 1000 );
}

MediaCellView::~MediaCellView()
{
    delete m_ui;
}

void
MediaCellView::metadataComputingStarted( const Media *media )
{
    if ( media != m_clip->getMedia() )
        return ;
    //Disable the delete button to avoid deleting the media while metadata are computed.
    m_ui->delLabel->setEnabled( false );
    //We don't need this event anymore now.
    disconnect( MetaDataManager::getInstance(), SIGNAL( startingComputing( const Media* )),
             this, SLOT( metadataComputingStarted( const Media* ) ) );
}

void
MediaCellView::metadataUpdated( const Media *media )
{
    setLength( media->lengthMS() );
    m_ui->thumbnail->setEnabled( true );
    //If the media is a Video or an Image, we must wait for the snapshot to be computer
    //before allowing deletion.
    if ( media->fileType() == Media::Audio )
        m_ui->delLabel->setEnabled( true );
}

void
MediaCellView::snapshotUpdated( const Media *media )
{
    setThumbnail( media->snapshot() );
    m_ui->delLabel->setEnabled( true );
}

void
MediaCellView::changeEvent( QEvent *e )
{
    QWidget::changeEvent( e );
    switch ( e->type() )
    {
    case QEvent::LanguageChange:
        m_ui->retranslateUi( this );
        nbClipUpdated();
        setLength( m_clip->lengthSecond() * 1000 );
        break;
    default:
        break;
    }
}

void
MediaCellView::setTitle( const QString& title )
{
    m_ui->title->setText( title );
}

void
MediaCellView::nbClipUpdated()
{
    quint32     nbClips;

    if ( m_clip->isRootClip() == true )
        nbClips = m_clip->getChilds()->count();
    else
        nbClips = m_clip->getParent()->getChilds()->count();

    if ( nbClips == 0 )
    {
        m_ui->clipCount->hide();
        m_ui->clipCountLabel->hide();
        m_ui->arrow->hide();
    }
    else
    {
        m_ui->clipCount->show();
        m_ui->clipCountLabel->show();
        m_ui->arrow->show();
    }
    //Use QString::number in any case, so we use the current locale.
    m_ui->clipCount->setText( QString::number( nbClips ) );
}

void
MediaCellView::setThumbnail( const QPixmap &pixmap )
{
    m_ui->thumbnail->setScaledContents( false );
    m_ui->thumbnail->setPixmap( pixmap.scaled( 64, 64, Qt::KeepAspectRatio ) );
}

const QPixmap*
MediaCellView::getThumbnail() const
{
    return m_ui->thumbnail->pixmap();
}

QString
MediaCellView::title() const
{
    return m_ui->title->text();
}

void
MediaCellView::mouseDoubleClickEvent( QMouseEvent* event )
{
    if ( ( event->buttons() | Qt::LeftButton ) == Qt::LeftButton )
    {
        ClipProperty* cp = new ClipProperty( m_clip, this );
        cp->setModal( true );
        cp->exec();
        delete cp;
    }
}

void
MediaCellView::mousePressEvent( QMouseEvent* event )
{
    QWidget::mousePressEvent( event );

    if ( ( event->buttons() | Qt::LeftButton ) == Qt::LeftButton )
    {
        m_dragStartPos = event->pos();
        emit cellSelected( m_clip->uuid() );
    }
}

void
MediaCellView::mouseMoveEvent( QMouseEvent* event )
{
    if ( ( event->buttons() | Qt::LeftButton ) != Qt::LeftButton )
         return;

    if ( ( event->pos() - m_dragStartPos ).manhattanLength()
          < QApplication::startDragDistance() )
        return;

    QMimeData* mimeData = new QMimeData;
    mimeData->setData( "vlmc/uuid", m_clip->fullId().toAscii() );
    QDrag* drag = new QDrag( this );
    drag->setMimeData( mimeData );
    const Media*  parent = m_clip->getMedia();
    drag->setPixmap( parent->snapshot().scaled( 100, 100, Qt::KeepAspectRatio ) );
    drag->exec( Qt::CopyAction | Qt::MoveAction, Qt::CopyAction );
}

void
MediaCellView::deleteButtonClicked( QWidget*, QMouseEvent* )
{
    if ( MainWorkflow::getInstance()->contains( m_clip->uuid() ) == true )
    {
        QMessageBox msgBox;
        msgBox.setText( tr( "This clip or some of its children are contained in the timeline." ) );
        msgBox.setInformativeText( tr( "Removing it will delete it from the timeline. Do you want to proceed?" ) );
        msgBox.setStandardButtons( QMessageBox::Ok | QMessageBox::Cancel );
        msgBox.setDefaultButton( QMessageBox::Ok );
        int     ret = msgBox.exec();

        switch ( ret )
        {
            case QMessageBox::Ok:
                break ;
            case QMessageBox::Cancel:
            default:
                return ;
        }
    }
    emit cellDeleted( m_clip );
}

void
MediaCellView::arrowButtonClicked( QWidget*, QMouseEvent* )
{
    emit arrowClicked( m_clip->uuid() );
}

void
MediaCellView::setLength( qint64 length, bool mSecs )
{
    QTime   duration;
    if ( mSecs )
        duration = duration.addMSecs( length );
    else
        duration = duration.addSecs( length );
    m_ui->length->setText( duration.toString( "hh:mm:ss" ) );
}

const QUuid&
MediaCellView::uuid() const
{
    return m_clip->uuid();
}

const Clip*
MediaCellView::clip() const
{
    return m_clip;
}

void
MediaCellView::contextMenuEvent( QContextMenuEvent *event )
{
    QMenu menu( this );

    //For now, as we only have the copy to workspace option, don't do anything if the clip
    //is not the root clip. Obviously, this will have to be removed if other actions are to be added.
    if ( m_clip->isRootClip() == false )
        return ;

    QAction* copyInWorkspace = menu.addAction( "Copy in workspace" );

    QAction* selectedAction = menu.exec( event->globalPos() );
    if ( selectedAction == NULL )
        return ;
    if ( copyInWorkspace == selectedAction )
    {
        if ( Workspace::getInstance()->copyToWorkspace( m_clip->getMedia() ) == false )
        {
            QMessageBox::warning( NULL, tr( "Can't copy to workspace" ),
                                  tr( "Can't copy this media to workspace: %1" ).arg( Workspace::getInstance()->lastError() ) );
        }
    }
}
