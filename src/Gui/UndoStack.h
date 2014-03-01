/*****************************************************************************
 * UndoStack.h: UndoStack For Undo/Redo Purposes
 *****************************************************************************
 * Copyright (C) 2008-2014 VideoLAN
 *
 * Authors: Christophe Courtaut <christophe.courtaut@gmail.com>
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

#ifndef UNDOSTACK_H
#define UNDOSTACK_H

#include <QUndoView>

#include "Commands/KeyboardShortcutHelper.h"
#include "Tools/QSingleton.hpp"

class QUndoStack;
class QUndoCommand;

class UndoStack : public QUndoView, public QSingleton<UndoStack>
{
    Q_OBJECT
    Q_DISABLE_COPY( UndoStack );

    public:
        void        push( QUndoCommand *command );
        void        beginMacro( const QString &text );
        void        endMacro();
        bool        canUndo();
        bool        canRedo();

    protected:
        void        changeEvent( QEvent *event );

    private:
        UndoStack( QWidget* parent );

        QUndoStack*     m_undoStack;

    public slots:
        void            clear();
        void            undo();
        void            redo();

    signals:
        void            cleanChanged( bool val );
        void            canRedoChanged( bool canRedo );
        void            canUndoChanged( bool canUndo );
        void            retranslateRequired();

    friend class        QSingleton<UndoStack>;
};

#endif // UNDOSTACK_H
