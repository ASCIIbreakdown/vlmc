/*****************************************************************************
 * MainWorkflow.h : Will query all of the track workflows to render the final
 *                  image
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
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

#ifndef MAINWORKFLOW_H
#define MAINWORKFLOW_H

#include "Singleton.hpp"
#include <QXmlStreamWriter>

class   Clip;
class   ClipHelper;
class   EffectsEngine;
class   Effect;
class   TrackHandler;
class   TrackWorkflow;
namespace   Workflow
{
    class   Frame;
    class   AudioSample;
}

class   QDomDocument;
class   QDomElement;
class   QMutex;
class   QReadWriteLock;

#include <QObject>
#include <QUuid>

/**
 *  \class  Represent the Timeline backend.
 */
class   MainWorkflow : public QObject, public Singleton<MainWorkflow>
{
    Q_OBJECT

    public:
        /**
         *  \struct     Represents an output, with both audio and video buffers.
         *              Note that an OutputBuffers will not necessarly have its both
         *              fields filed out.
         */
        struct      OutputBuffers
        {
            Workflow::Frame         *video;
            Workflow::AudioSample   *audio;
        };
        /**
         *  \enum   Represents the potential Track types.
         */
        enum    TrackType
        {
            VideoTrack, ///< Represents a video track
            AudioTrack, ///< Represents an audio track
            NbTrackType, ///< Used to know how many types we have
        };
        /**
         *  \enum   Used to know which part required a change of rendered frame.
         *          The main use of this enum is to avoid infinite information propagation
         *          such as the timeline informing itself that the frame as changed, which
         *          would cause a signal to be emmited to inform every other part that the
         *          rendered frame has changed, and so on.
         */
        enum    FrameChangedReason
        {
            Renderer, ///< Used by the WorkflowRenderer
            /**
             *  \brief      Used by the timeline cursor.
             *  \warning    The timeline cursor is not the timeline ruler
             */
            TimelineCursor,
            PreviewCursor, ///< Used by the preview widget, when using the time cursor.
            RulerCursor, ///< Used by the timeline's ruler.
        };

        /**
         *  \brief      Add a clip to the workflow
         *
         *  When called, this method will emit
         *  clipAdded( Clip*, unsigned int, qint64, MainWorkflow::TrackType ) SIGNAL.
         *  \param      clip    The clip to add
         *  \param      trackId The number of the track (starting at 0)
         *  \param      start   The clip's starting position
         *  \param      type    The track type (audio or video)
         *  \param      informGui  Will be false if the action is queried by the GUI.
         *                              true otherwise.
         *  \sa         removeClip( const QUuid&, unsigned int, MainWorkflow::TrackType )
         *  \sa         clipAdded( Clip*, unsigned int, qint64, MainWorkflow::TrackType )
         */
        void            addClip( ClipHelper* clipHelper, quint32 trackId, qint64 start,
                                         TrackType type, bool informGui );


        void            addEffect( Effect* effect, quint32 trackId, const QUuid &uuid,
                                   TrackType type );

        /**
         *  \brief      Initialize the workflow for the render.
         *
         *  \param      width   The width to use with this render session.
         *  \param      height  The height to use with this render session.
         *  This will basically activate all the tracks, so they can render.
         */
        void                    startRender( quint32 width, quint32 height, double fps );
        /**
         *  \brief      Gets a frame from the workflow
         *
         *  \return A pointer to an output buffer. This pointer must NOT be released.
         *          Both types of this output buffer are not guarantied to be filled.
         *          However, if VideoTrack was passed as trackType, the video member of
         *          the output buffer is guarantied to be filled. The same applies for
         *          AudioTrack
         *  \param  trackType   The type of track you wish to get the render from.
         *  \param  paused      The paused state of the renderer
         */
        OutputBuffers*          getOutput( TrackType trackType, bool paused );
        /**
         *  \brief              Set the workflow position by the desired frame
         *  \param              currentFrame: The desired frame to render from
         *  \param              reason: The program part which required this frame change
                                        (to avoid cyclic events)
        */
        void                    setCurrentFrame( qint64 currentFrame,
                                                MainWorkflow::FrameChangedReason reason );

        /**
         *  \brief              Get the workflow length in frames.
         *  \return             Returns the global length of the workflow
         *                      in frames.
        */
        qint64                  getLengthFrame() const;

        /**
         *  \brief              Get the currently rendered frame.
         *  \return             Returns the current frame.
         */
        qint64                  getCurrentFrame() const;

        /**
         *  \brief      Stops the rendering.
         *
         *  This will stop every ClipWorkflow that are currently rendering.
         *  Calling this methid will cause the workflow to return to frame 0, and emit
         *  the signal frameChanged(), with the reason: Renderer
         */
        void                    stop();

        /**
         *  \brief              Unconditionnaly switch to the next frame.
         *
         *  \param  trackType   The type of the frame counter to increment.
         *                      Though it seems odd to speak about frame for AudioTrack,
         *                      it's mainly a render position used for
         *                      synchronisation purpose.
         *  \sa         previousFrame( MainWorkflow::TrackType );
         */
        void                    nextFrame( TrackType trackType );
        /**
         *  \brief      Unconditionnaly switch to the previous frame.
         *
         *  \param      trackType   The type of the frame counter to decrement.
         *                          Though it seems odd to speak about frame for
         *                          AudioTrack, it's mainly a render position used for
         *                          synchronisation purpose.
         *  \sa         nextFrame( MainWorkflow::TrackType );
         */
        void                    previousFrame( TrackType trackType );

        /**
         *  \brief              Remove a clip from the workflow
         *
         *  The created ClipWorkflow is deleted, and the added clip is returned.
         *  Calling this method will cause
         *  clipRemoved( Clip*, unsigned int, MainWorkflow::TrackType ) to be emitted.
         *  \param  uuid        The uuid of the clip to remove.
         *  \param  trackId     The id of the track containing the clip to remove
         *  \param  trackType   The type of the track containing the clip to remove
         *  \sa                 addClip( Clip*, unsigned int, qint64, TrackType )
         *  \sa                 clipRemoved(Clip*, unsigned int, MainWorkflow::TrackType)
         */
        Clip*                   removeClip( const QUuid& uuid, unsigned int trackId,
                                            MainWorkflow::TrackType trackType );

        /**
         *  \brief              Move a clip in the workflow
         *
         *  This will move a clip, potentially from a track to anoher, to a new
         *  starting position.
         *  if undoRedoCommand is true, the
         *  clipMoved( QUuid, unsigned int, qint64, MainWorkflow::TrackType ) will be
         *  emitted.
         *  This (bad) behaviour is caused by the fact that this move is mostly required
         *  by the timeline, which has already move its graphic item.
         *  When caused by an undo redo command, the timeline isn't aware of the change,
         *  and needs to be.
         *  \param      uuid        The uuid of the clip to move
         *  \param      oldTrack    The origin track of the clip
         *  \param      newTrack    The destination track for the clip
         *  \param      pos         The new starting position for the clip
         *  \param      trackType   The type of the track containing the clip to move
         *  \param      undoRedoCommand Must be true if the method is called from an
         *                              undo/redo action. If any doubt, false seems like
         *                              a good choice.
         *  \todo       Fix the last parameter. Such a nasty hack shouldn't even exist.
         *  \sa         clipMoved( QUuid, unsigned int, qint64, MainWorkflow::TrackType )
         */
        void                    moveClip( const QUuid& uuid, unsigned int oldTrack,
                                          unsigned int newTrack, qint64 pos,
                                          MainWorkflow::TrackType trackType,
                                          bool undoRedoCommand = false );
        /**
         *  \brief              Return the given clip position.
         *
         *  \param      uuid        The clip uuid
         *  \param      trackId     The track containing the clip
         *  \param      trackType   The type of the track containing the clip
         *  \return                 The given clip position, in frame. If not found, -1
         *                          is returned.
         */
        qint64                  getClipPosition( const QUuid& uuid, unsigned int trackId,
                                                MainWorkflow::TrackType trackType ) const;

        /**
         *  \brief      Mute a track.
         *
         *  A muted track will not be asked for render. It won't even emit endReached
         *  signal. To summerize, a mutted track is an hard deactivated track.
         *  \param  trackId     The id of the track to mute
         *  \param  trackType   The type of the track to mute.
         *  \sa     unmuteTrack( unsigned int, MainWorkflow::TrackType );
         */
        void                    muteTrack( unsigned int trackId,
                                           MainWorkflow::TrackType trackType );
        /**
         *  \brief      Unmute a track.
         *
         *  \param  trackId     The id of the track to unmute
         *  \param  trackType   The type of the track to unmute.
         *  \sa     muteTrack( unsigned int, MainWorkflow::TrackType );
         */
        void                    unmuteTrack( unsigned int trackId,
                                             MainWorkflow::TrackType trackType );

        /**
         *  \brief      Mute a clip.
         *
         *  \param  uuid        The clip's uuid.
         *  \param  trackId     The id of the track containing the clip.
         *  \param  trackType   The type of the track containing the clip.
         */
        void                    muteClip( const QUuid& uuid, unsigned int trackId,
                                          MainWorkflow::TrackType trackType );

        /**
         *  \brief      Unmute a clip.
         *
         *  \param  uuid        The clip's uuid.
         *  \param  trackId     The id of the track containing the clip.
         *  \param  trackType   The type of the track containing the clip.
         */
        void                    unmuteClip( const QUuid& uuid, unsigned int trackId,
                                          MainWorkflow::TrackType trackType );

        /**
         *  \brief              Get the number of track for a specific type
         *
         *  \param  trackType   The type of the tracks to count
         *  \return             The number of track for the type trackType
         */
        int                     getTrackCount( MainWorkflow::TrackType trackType ) const;

        /**
         *  \brief      Get the width used for rendering.
         *
         *  This value is used by the ClipWorkflow that generates the frames.
         *  If this value is edited in the preferences, it will only change after the
         *  current render has been stopped.
         *  \return     The width (in pixels) of the currently rendered frames
         *  \sa         getHeight()
         */
        quint32                getWidth() const;
        /**
         *  \brief      Get the height used for rendering.
         *
         *  This value is used by the ClipWorkflow that generates the frames.
         *  If this value is edited in the preferences, it will only change after the
         *  current render has been stopped.
         *  \return     The height (in pixels) of the currently rendered frames
         *  \sa         getWidth()
         */
        quint32                getHeight() const;

        /**
         *  \brief          Will render one frame only
         *
         *  It will change the ClipWorkflow frame getting mode from Get to Pop, just for
         *  one frame
         */
        void                    renderOneFrame();

        /**
         *  \brief              Set the render speed.
         *
         *  This will activate or deactivate vlc's pace-control
         *  \param  val         If true, ClipWorkflow will use no-pace-control
         *                      else, pace-control.
         */
        void                    setFullSpeedRender( bool val );

        ClipHelper*             split( ClipHelper* toSplit, ClipHelper* newClip, quint32 trackId,
                                       qint64 newClipPos, qint64 newClipBegin,
                                       MainWorkflow::TrackType trackType );

        void                    resizeClip( ClipHelper* clipHelper, qint64 newBegin, qint64 newEnd,
                                          qint64 newPos, quint32 trackId,
                                          MainWorkflow::TrackType trackType,
                                          bool undoRedoAction = false );

        void                    unsplit( ClipHelper* origin, ClipHelper* splitted, quint32 trackId,
                                         MainWorkflow::TrackType trackType );

        /**
         *  \return     true if the current workflow contains the clip which the uuid was
         *              passed. Falsed otherwise.
         *
         *  \param      uuid    The Clip uuid, not the ClipHelper's.
         */
        bool                    contains( const QUuid& uuid ) const;

        /**
         *  \brief      Stop the frame computing process.
         *
         *  This will stop all the currently running ClipWorkflow.
         *  This is meant to be called just before the stop() method.
         */
        void                    stopFrameComputing();

        /// Pre-filled buffer used when there's nothing to render
        static Workflow::Frame*         blackOutput;

    private:
        MainWorkflow( int trackCount = 64 );
        ~MainWorkflow();
        /**
         *  \brief  Compute the length of the workflow.
         *
         *  This is meant to be used internally, after an operation occurs on the workflow
         *  such as adding a clip, removing it...
         *  This method will update the attribute m_lengthFrame
         */
        void                    computeLength();

        /**
         * \param   uuid : The clip's uuid.
         *              Please note that the UUID must be the real clip's uuid,
         *              and not the timeline (ClipHelper) uuid, or else nothing would match.
         *  \param  trackId : the track id
         *  \param  trackType : the track type (audio or video)
         *  \returns    The clip that matches the given UUID, or NULL.
         */
        Clip*                   getClip( const QUuid& uuid, unsigned int trackId,
                                         MainWorkflow::TrackType trackType );

    private:
        /// Lock for the m_currentFrame atribute.
        QReadWriteLock*                 m_currentFrameLock;
        /**
         *  \brief  An array of currently rendered frame.
         *
         *  This must be indexed with MainWorkflow::TrackType.
         *  The Audio array entry is designed to synchronize the renders internally, as it
         *  is not actually a frame.
         *  If you wish to know which frame is really rendered, you must use
         *  m_currentFrame[MainWorkflow::VideoTrack], which is the value that will be used
         *  when setCurrentFrame() is called.
         */
        qint64*                         m_currentFrame;
        /// The workflow length, in frame.
        qint64                          m_lengthFrame;
        /// This boolean describe is a render has been started
        bool                            m_renderStarted;
        QMutex*                         m_renderStartedMutex;

        /// Contains the trackhandler, indexed by MainWorkflow::TrackType
        TrackHandler**                  m_tracks;
        /// Pre-allocated buffer, that will contain every computed outputs.
        OutputBuffers*                  m_outputBuffers;

        /// Width used for the render
        quint32                         m_width;
        /// Height used for the render
        quint32                         m_height;

        friend class                    Singleton<MainWorkflow>;

    private slots:
        /**
         *  \brief  Called when a track end is reached
         *
         *  If all track has reached end, the mainWorkflowEndReached() signal will be
         *  emitted;
         *  \sa     mainWorkflowEndReached()
         */
        void                            tracksEndReached();

    public slots:
        /**
         *  \brief  load a project based on a QDomElement
         *
         *  \param  project             The project node to load.
         */
        void                            loadProject( const QDomElement& project );
        /**
         *  \brief          Save the project on a XML stream.
         *
         *  \param  project The XML stream representing the project
         */
        void                            saveProject( QXmlStreamWriter& project ) const;
        /**
         *  \brief      Clear the workflow.
         *
         *  Calling this method will cause every clip workflow to be deleted, along with
         *  the associated Clip.
         *  This method will emit cleared() signal once finished.
         *  \sa     removeClip( const QUuid&, unsigned int, MainWorkflow::TrackType )
         *  \sa     cleared()
         */
        void                            clear();

    signals:
        /**
         *  \brief      Used to notify a change to the timeline and preview widget cursor
         *
         *  \param      newFrame    The new rendered frame
         *  \param      reason      The reason for changing frame. Usually, if emitted
         *                          from the MainWorkflow, this should be "Renderer"
         */
        void                    frameChanged( qint64 newFrame,
                                              MainWorkflow::FrameChangedReason reason );

        /**
         *  \brief  Emitted when workflow end is reached
         *
         *  Workflow end is reached when tracksEndReached() is called, and no more tracks
         *  are activated (ie. they all reached end)
         */
        void                    mainWorkflowEndReached();
        /**
         *  \brief  Emitted when a clip is added
         *
         *  \param  clip        The clip that has been added
         *  \param  trackId     The id of the track in which the clip has been added
         *  \param  pos         The position of the new Clip
         *  \param  trackType   The type of the clip that has been added
         *  \sa                 addClip( Clip*, unsigned int, qint64, TrackType )
         */
        void                    clipAdded( ClipHelper* clip, unsigned int trackId,
                                           qint64 pos, MainWorkflow::TrackType trackType );
        /**
         *  \brief              Emitted when a clip is removed
         *  \param  uuid        The clip that has been removed's uuid.
         *  \param  trackId     The track that was containing the clip
         *  \param  trackType   The type of the track that was containing the clip
         *  \sa     removeClip( const QUuid&, unsigned int, MainWorkflow::TrackType )
         */
        void                    clipRemoved( const QUuid& uuid, unsigned int trackId,
                                             MainWorkflow::TrackType trackType );
        /**
         *  \brief              Emitted when a clip has been moved
         *
         *  \param  uuid        The uuid of the moved clip
         *  \param  trackid     The destination track of the moved media
         *  \param  pos         The clip new position
         *  \param  trackType   The moved clip type.
         *  \sa                 moveClip( const QUuid&, unsigned int, unsigned int,
         *                                  qint64, MainWorkflow::TrackType, bool );
         *  \warning            This is not always emitted. Check removeClip for more
         *                      details
         */
        void                    clipMoved( const QUuid& uuid, unsigned int trackId,
                                          qint64 pos, MainWorkflow::TrackType trackType );
        /**
         *  \brief  Emitted when the workflow is cleared.
         *
         *  \sa     clear();
         */
        void                    cleared();

        /**
         *  \brief  Emitted when the global length of the workflow changes.
         *
         *  \param  newLength   The new length, in frames
         */
        void                    lengthChanged( qint64 );
};

#endif // MAINWORKFLOW_H
