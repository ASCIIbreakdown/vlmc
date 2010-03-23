/*****************************************************************************
 * TracksView.h: QGraphicsView that contains the TracksScene
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include "vlmc.h"
#include "MainWorkflow.h"
#include "AbstractGraphicsMediaItem.h"
#include "GraphicsCursorItem.h"

#include <QWidget>
#include <QGraphicsView>

class QWheelEvent;
class QGraphicsWidget;
class QGraphicsLinearLayout;

class TracksScene;
class GraphicsMovieItem;
class GraphicsAudioItem;
class WorkflowRenderer;

class ItemPosition
{
public:
    ItemPosition() : m_track( -1 ), m_time( -1 )
    {

    }
    ItemPosition( qint32 track, qint64 time )
    {
        m_track = track;
        m_time = time;
    }

    qint32      track()
    {
        return m_track;
    }
    qint64      time()
    {
        return m_time;
    }

    void        setTrack( qint32 track )
    {
        m_track = track;
    }
    void        setTime( qint64 time )
    {
        m_time = time;
    }

    bool        isValid()
    {
        if ( m_track < 0 || m_time < 0 )
            return false;
        return true;
    }

private:
    qint32      m_track;
    qint64      m_time;
};

/**
 * \brief Class managing the timeline using QGraphicsItems.
 */
class TracksView : public QGraphicsView
{
    Q_OBJECT

public:
    TracksView( QGraphicsScene *scene, MainWorkflow *mainWorkflow, WorkflowRenderer *renderer, QWidget *parent = 0 );
    /**
     * \brief Set the duration of the project.
     * \param duration Duration in frames.
     * \sa duration
     */
    void setDuration( int duration );
    /**
     * \brief Get the duration of the project.
     * \return The duration in frames.
     * \sa setDuration
     */
    int duration() const { return m_projectDuration; }
    /**
     * \brief Get the current tracks' height.
     * \return The size in pixels.
     */
    int tracksHeight() const { return m_tracksHeight; }
    /**
     * \brief Change the position of the cursor.
     * \param pos The position where to move the cursor, in frames.
     * \sa cursorPos
     */
    void setCursorPos( qint64 pos );
    /**
     * \brief Get the current position of the cursor.
     * \return The current frame;
     * \sa setCursorPos
     */
    qint64 cursorPos();
    /**
     * \brief Return a pointer to the cursor.
     * \return A pointer to the GraphicsCursorItem.
     */
    GraphicsCursorItem *tracksCursor() const { return m_cursorLine; }
    /**
     * \brief Change the scale factor of the timeline.
     * \sa Timeline::changeZoom
     */
    void setScale( double scaleFactor );
    /**
     * \brief Return the list of all the AbstractGraphicsMediaItem contained
     *        in the timeline at the given position.
     * \param pos The position to look at.
     * \return A list of pointer to AbstractGraphicsMediaItem.
     * \warning Calling this method can be excessively slow!
     * \sa mediaItems()
     */
    QList<AbstractGraphicsMediaItem*> mediaItems( const QPoint &pos );
    /**
     * \brief This is an overloaded method provided for convenience.
     * \warning Calling this method can be excessively slow!
     * \sa mediaItems( const QPoint& pos )
     */
    QList<AbstractGraphicsMediaItem*> mediaItems();
    /**
     * \brief Remove multiple items from the timeline.
     * \param items A QList of pointers to AbstractGraphicsMediaItem.
     * \sa removeMediaItem( AbstractGraphicsMediaItem* )
     */
    void                    removeMediaItem( const QList<AbstractGraphicsMediaItem*> &items );
    /**
     * \brief Change the currently selected tool.
     * \param button The selected tool button.
     * \sa tool, ToolButtons
     */
    void                    setTool( ToolButtons button );
    /**
     * \brief Return the currently selected tool.
     * \return Selected tool button.
     * \sa setTool, ToolButtons
     */
    ToolButtons             tool() { return m_tool; }
    /**
     * \brief Get the WorkflowRenderer used by the timeline.
     * \return A pointer to the current WorkflowRenderer.
     */
    WorkflowRenderer        *getRenderer() { return m_renderer; }
    /**
     * \brief Ugly hack to change the old track number of an item.
     * \deprecated Do not use, will be removed soon.
     */
    bool                    setItemOldTrack( const QUuid &uuid, quint32 oldTrackNumber );

public slots:
    /**
     * \brief Remove all items from the timeline.
     * \sa Timeline::clear
     */
    void                    clear();
    /**
     * \brief Remove a Media from the timeline (and from the backend).
     * \param uuid The unique identifier of the Media.
     */
    void                    deleteMedia( const QUuid& uuid );
    /**
     * \brief Insert an item into the timeline.
     * \param clip Clip to insert.
     * \param track The track's number.
     * \param trackType The type of the track (Audio or Video)
     * \param start The position in frames.
     */
    void                    addMediaItem( Clip *clip, unsigned int track, MainWorkflow::TrackType trackType, qint64 start );
    /**
     * \brief Move an item in the timeline.
     * \param uuid The Universally Unique Identifier of the item.
     * \param track The new track of the item.
     * \param time The new position (in frames) of the item.
     */
    void                    moveMediaItem( const QUuid &uuid, unsigned int track, qint64 time );
    /**
     * \brief Remove an item from the timeline.
     * \param uuid The Universally Unique Identifier of the item.
     * \param track The current track of the item.
     * \param trackType The type of the track (Audio or Video)
     */
    void                    removeMediaItem( const QUuid &uuid, unsigned int track, MainWorkflow::TrackType trackType );
    /**
     * \brief This is an overloaded method provided for convenience.
     * \param item A pointer to AbstractGraphicsMediaItem.
     * \sa removeMediaItem( const QList<AbstractGraphicsMediaItem*>& )
     */
    void                    removeMediaItem( AbstractGraphicsMediaItem *item );

protected:
    virtual void            resizeEvent( QResizeEvent *event );
    virtual void            drawBackground( QPainter *painter, const QRectF &rect );
    virtual void            mouseMoveEvent( QMouseEvent *event );
    virtual void            mousePressEvent( QMouseEvent *event );
    virtual void            mouseReleaseEvent( QMouseEvent *event );
    virtual void            wheelEvent( QWheelEvent *event );
    virtual void            dragEnterEvent( QDragEnterEvent *event );
    virtual void            dragMoveEvent( QDragMoveEvent *event );
    virtual void            dragLeaveEvent( QDragLeaveEvent *event );
    virtual void            dropEvent( QDropEvent *event );

private slots:
    /**
     * \brief Ensure that the cursor is visible.
     */
    void                    ensureCursorVisible();

    /**
     * \brief Return the visible area of the viewport.
     */
    QRectF                  visibleRect();
    /**
     * \brief Update the global duration of the project.
     * This method should be called when an item is inserted, moved or removed to ensure
     * that the global time calculation is up-to-date.
     */
    void                    updateDuration();

    /**
     * \brief PLEASE DOCUMENT ME
     */
    void                    cleanUnusedTracks();
    /**
     * \brief Split an item in two at the given position.
     * \details Internally, the item given as parameter will be shrinked and a new one will be
     * appended at the end.
     * \param item The item.
     * \param frame the frame number where the cut should takes place.
     */
    void                    split( AbstractGraphicsMediaItem *item, qint64 frame );

private:
    /**
     * \brief Create the initial layout of the tracks
     */
    void                    createLayout();
    /**
     * \brief Insert an empty video track.
     */
    void                    addVideoTrack();
    /**
     * \brief Insert an empty audio track.
     */
    void                    addAudioTrack();
    /**
     * \brief DOCUMENT ME
     */
    void                    removeVideoTrack();
    /**
     * \brief DOCUMENT ME
     */
    void                    removeAudioTrack();
    /**
     * \brief DOCUMENT ME
     */
    void                    cleanTracks( MainWorkflow::TrackType type );

    /**
     * \brief This is an overloaded method provided for convenience.
     * \param item Item to move.
     * \param position New position of the item.
     * \sa moveMediaItem( const QUuid& uuid, unsigned int track, qint64 time );
     */
    void                    moveMediaItem( AbstractGraphicsMediaItem *item, QPoint position );
    /**
     * \brief This is an overloaded method provided for convenience.
     * \param item Item to move.
     * \param track The new track of the item.
     * \param time The new position (in frames) of the item.
     * \sa moveMediaItem( const QUuid& uuid, unsigned int track, qint64 time );
     */
    void                    moveMediaItem( AbstractGraphicsMediaItem *item, quint32 track, qint64 time );

    ItemPosition            findPosition( AbstractGraphicsMediaItem *item, quint32 track, qint64 time );

    /**
     * \brief Return a pointer to the specified track.
     * \param type The track's type.
     * \param number The track number.
     * \return A pointer to the GraphicsTrack.
     */
    GraphicsTrack           *getTrack( MainWorkflow::TrackType type, unsigned int number );
    QGraphicsScene          *m_scene;
    int                     m_tracksHeight;
    int                     m_projectDuration;
    GraphicsCursorItem      *m_cursorLine;
    QGraphicsLinearLayout   *m_layout;
    quint32                 m_numVideoTrack;
    quint32                 m_numAudioTrack;
    MainWorkflow            *m_mainWorkflow;
    GraphicsMovieItem       *m_dragVideoItem;
    GraphicsAudioItem       *m_dragAudioItem;
    QGraphicsWidget         *m_separator;
    ToolButtons             m_tool;
    WorkflowRenderer        *m_renderer;

    // Mouse actions on Medias
    bool                    m_actionMove;
    bool                    m_actionMoveExecuted;
    bool                    m_actionResize;
    qint64                  m_actionResizeStart;
    qint64                  m_actionResizeBase;
    qint64                  m_actionResizeOldBegin;
    int                     m_actionRelativeX;
    AbstractGraphicsMediaItem::From m_actionResizeType;
    AbstractGraphicsMediaItem       *m_actionItem;

signals:
    /**
     * \brief Emitted when the zoom level has changed.
     */
    void                    zoomIn();
    /**
     * \brief Emitted when the zoom level has changed.
     */
    void                    zoomOut();
    /**
     * \brief Emitted when a new duration has been computed.
     */
    void                    durationChanged( int duration );
    /**
     * \brief Emitted when a video track has been added.
     * \param track A pointer to the newly added track.
     */
    void                    videoTrackAdded( GraphicsTrack *track );
    /**
     * \brief Emitted when an audio track has been added.
     * \param track A pointer to the newly added track.
     */
    void                    audioTrackAdded( GraphicsTrack *track );

    /**
     * \brief DOCUMENT ME
     */
    void                    videoTrackRemoved();
    /**
     * \brief DOCUMENT ME
     */
    void                    audioTrackRemoved();

friend class Timeline;
friend class TracksScene;
friend class AbstractGraphicsMediaItem;
};

#endif // TRACKSVIEW_H
