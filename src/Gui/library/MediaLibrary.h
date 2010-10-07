/*****************************************************************************
 * MediaLibrary.h: VLMC media library's ui
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <beauze.h@gmail.com>
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

#ifndef MEDIALIBRARY_H
#define MEDIALIBRARY_H

#include <QWidget>

#include "ui_MediaLibrary.h"
class   Clip;
class   MediaListView;
class   ViewController;

class MediaLibrary : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY( MediaLibrary );

    public:
        typedef bool    (*Filter)( const Clip*, const QString& filter );
        explicit MediaLibrary( QWidget *parent = 0);

    protected:
        void        dragEnterEvent( QDragEnterEvent *event );
        void        dragMoveEvent( QDragMoveEvent *event );
        void        dragLeaveEvent( QDragLeaveEvent *event );
        void        dropEvent( QDropEvent *event );

    private:
        /**
         *  \return     The appropriate filter function
         *
         *  This will evaluate the currently selected filter, and return the appropriate
         *  function.
         */
        Filter              currentFilter();

    //Filters list :
        static bool         filterByName( const Clip *clip, const QString &filter );
        static bool         filterByTags( const Clip *clip, const QString &filter );

    private:
        Ui::MediaLibrary    *m_ui;
        MediaListView       *m_mediaListView;

    private slots:
        void                filterUpdated( const QString &filter );
        /**
         *  \brief          Used to update the filters when the view is changed.
         *
         *  A view is changed when the user goes through the clips hierarchy.
         */
        void                viewChanged( ViewController* view );
        void                filterTypeChanged();

    signals:
        void                importRequired();
        void                clipSelected( Clip* );
};

#endif // MEDIALIBRARY_H
