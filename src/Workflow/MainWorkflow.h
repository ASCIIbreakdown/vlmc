/*****************************************************************************
 * MainWorkflow.h : Will query all of the track workflows to render the final
 *                  image
 *****************************************************************************
 * Copyright (C) 2008-2009 the VLMC team
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

#include <QObject>
#include <QReadWriteLock>
#include <QMutex>
#include <QDomElement>
#include <QWaitCondition>

#include "Singleton.hpp"
#include "Clip.h"
#include "LightVideoFrame.h"
#include "EffectsEngine.h"
#include "AudioClipWorkflow.h"

class   TrackWorkflow;
class   TrackHandler;

class   MainWorkflow : public QObject, public Singleton<MainWorkflow>
{
    Q_OBJECT

    public:
        struct      OutputBuffers
        {
            const LightVideoFrame*              video;
            AudioClipWorkflow::AudioSample*     audio;
        };
        enum    TrackType
        {
            BothTrackType = -1,
            VideoTrack,
            AudioTrack,
            NbTrackType,
        };
        enum    FrameChangedReason
        {
            Renderer,
            TimelineCursor,
            PreviewCursor,
            RulerCursor,
        };

        void                    addClip( Clip* clip, unsigned int trackId, qint64 start, TrackType type );

        void                    startRender();
        void                    getOutput( TrackType trackType );
        OutputBuffers*          getSynchroneOutput( TrackType trackType );
        EffectsEngine*          getEffectsEngine();

        /**
         *  \brief              This method is meant to make the workflow go to the next frame, only in rendering mode.
         *                      The nextFrame() method will always go for the next frame, whereas this one only does when
         *                      rendering isn't paused.
         */
        void                    goToNextFrame( MainWorkflow::TrackType trackype );

        /**
         *  \brief              Set the workflow position by the desired frame
         *  \param              currentFrame: The desired frame to render from
         *  \paragraph          reason: The program part which required this frame change
                                        (to avoid cyclic events)
        */
        void                    setCurrentFrame( qint64 currentFrame,
                                                 MainWorkflow::FrameChangedReason reason );

        /**
         *  \return             Returns the global length of the workflow
         *                      in frames.
        */
        qint64                  getLengthFrame() const;

        /**
         *  \return             Returns the current frame.
         */
        qint64                  getCurrentFrame() const;

        /**
         *  Stop the workflow (including sub track workflows and clip workflows)
         */
        void                    stop();

        /**
         *  Pause the main workflow and all its sub-workflows
         */
        void                    pause();
        void                    unpause();

        void                    nextFrame( TrackType trackType );
        void                    previousFrame( TrackType trackType );

        Clip*                   removeClip( const QUuid& uuid, unsigned int trackId, MainWorkflow::TrackType trackType );

        void                    moveClip( const QUuid& uuid, unsigned int oldTrack,
                                          unsigned int newTrack, qint64 pos,
                                          MainWorkflow::TrackType trackType, bool undoRedoCommand = false );
        qint64                  getClipPosition( const QUuid& uuid, unsigned int trackId, MainWorkflow::TrackType trackType ) const;

        /**
         *  \brief  This method will wake every wait condition, so that threads won't
         *          be waiting anymore, thus avoiding dead locks.
         */
        void                    cancelSynchronisation();

        void                    muteTrack( unsigned int trackId, MainWorkflow::TrackType );
        void                    unmuteTrack( unsigned int trackId, MainWorkflow::TrackType );

        /**
         * \param   uuid : The clip's uuid.
         *              Please note that the UUID must be the "timeline uuid"
         *              and note the clip's uuid, or else nothing would match.
         *  \param  trackId : the track id
         *  \param  trackType : the track type (audio or video)
         *  \returns    The clip that matches the given UUID.
         */
        Clip*                   getClip( const QUuid& uuid, unsigned int trackId, MainWorkflow::TrackType trackType );

        void                    setFullSpeedRender( bool value );
        int                     getTrackCount( MainWorkflow::TrackType trackType ) const;

    private:
        MainWorkflow( int trackCount = 64 );
        ~MainWorkflow();
        void                    computeLength();
        void                    activateTrack( unsigned int trackId );

        static LightVideoFrame* blackOutput;

    private:
        QReadWriteLock*                 m_currentFrameLock;
        qint64*                         m_currentFrame;
        qint64                          m_lengthFrame;
        /**
         *  This boolean describe is a render has been started
        */
        bool                            m_renderStarted;
        QReadWriteLock*                 m_renderStartedLock;

        QMutex*                         m_renderMutex;
        QWaitCondition*                 m_synchroneRenderWaitCondition;
        QMutex*                         m_synchroneRenderWaitConditionMutex;
        unsigned int                    m_nbTrackHandlerToRender;
        QMutex*                         m_nbTrackHandlerToRenderMutex;
        bool                            m_paused;
        TrackHandler**                  m_tracks;
        OutputBuffers*                  m_outputBuffers;

        EffectsEngine*                  m_effectEngine;

        friend class    Singleton<MainWorkflow>;

    private slots:
        void                            tracksPaused();
        void                            tracksUnpaused();
        void                            tracksRenderCompleted();
        void                            tracksEndReached();

    public slots:
        void                            loadProject( const QDomElement& project );
        void                            saveProject( QDomDocument& doc, QDomElement& rootNode );
        void                            clear();

    signals:
        /**
         *  \brief Used to notify a change to the timeline and preview widget cursor
         */
        void                    frameChanged( qint64,
                                              MainWorkflow::FrameChangedReason );

        void                    mainWorkflowEndReached();
        void                    mainWorkflowPaused();
        void                    mainWorkflowUnpaused();
        void                    clipAdded( Clip*, unsigned int, qint64, MainWorkflow::TrackType );
        void                    clipRemoved( Clip*, unsigned int, MainWorkflow::TrackType );
        void                    clipMoved( QUuid, unsigned int, qint64, MainWorkflow::TrackType );
        void                    cleared();
};

#endif // MAINWORKFLOW_H
