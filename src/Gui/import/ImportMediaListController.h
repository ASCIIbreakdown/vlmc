/*****************************************************************************
 * ImportMediaListController.h
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Geoffroy Lacarriere <geoffroylaca@gmail.com>
 *          Thomas Boquet <thomas.boquet@gmail.com>
 *          Clement CHAVANCE <chavance.c@gmail.com>
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

#ifndef IMPORTMEDIALISTCONTROLLER_H
#define IMPORTMEDIALISTCONTROLLER_H

#include "StackViewController.h"
#include "ListViewController.h"
#include "Media.h"
#include "Clip.h"

class   MediaCellView;

class ImportMediaListController : public ListViewController
{
    Q_OBJECT

    public:
        ImportMediaListController( StackViewController* nav );
        ~ImportMediaListController();
        void    addMedia( Media* media );
        void    removeMedia( const QUuid& uuid );
        void    addClip( Clip* clip );
        void    removeClip( const QUuid& uuid );
        void    cleanAll();
        void    addClipsFromMedia( Media* media );

        const QHash<QUuid, MediaCellView*>* mediaCellList() const;
        MediaCellView*  cell( QUuid uuid ) const;
        bool    contains( QUuid uuid );
        int     nbDeletions() const { return m_clipDeleted; }

    private:
        StackViewController*                m_nav;
        QHash<QUuid, MediaCellView*>*       m_mediaCellList;
        int                                 m_clipDeleted;

    public slots:
        void    metaDataComputed( const Media* media );
        void    clipDeletion( const QUuid& uuid );
        void    clipAdded( Clip* clip );

    signals:
        void    mediaSelected( const QUuid& uuid );
        void    clipSelected( const QUuid& uuid );
        void    mediaDeleted( const QUuid& uuid );
        void    clipDeleted( const QUuid& uuid );
        void    showClipListAsked( const QUuid& uuid );

};

#endif // IMPORTMEDIALISTCONTROLLER_H
