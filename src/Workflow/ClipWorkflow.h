 /*****************************************************************************
 * ClipWorkflow.h : Clip workflow. Will extract a single frame from a VLCMedia
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

#ifndef CLIPWORKFLOW_H
#define CLIPWORKFLOW_H

#include "mdate.h"
#include "ClipHelper.h"

#include <QObject>
#include <QUuid>
#include <QXmlStreamWriter>

class   QMutex;
class   QReadWriteLock;
class   QWaitCondition;

class   Clip;
class   WaitCondition;

namespace Workflow
{
    class   Frame;
}

namespace LibVLCpp
{
    class   MediaPlayer;
    class   Media;
}

class   ClipWorkflow : public QObject
{
    Q_OBJECT

    public:
        enum        State
        {
            None = -1,
            /// \brief  Used when the clipworkflow hasn't been started yet
            Stopped,            //0
            /**
             *  \brief  Used when initializing is in progress (until the mediaplayer
             *          enters the playing state.
             */
            Initializing,       //1
            /// \brief  Used when the clipworkflow is launched and active
            Rendering,          //2
            /// \brief  Used when stopping
            Stopping,           //3
            /// \brief  Used when end is reached, IE no more frame has to be rendered, but the trackworkflow
            ///         may eventually ask for some.
            EndReached,         //4
            // Here starts internal states :
            /// \brief  This state will be used when an unpause
            ///         has been required
            UnpauseRequired,    //5
            /// \brief  This state will be used when a pause
            ///         has been required
            PauseRequired,      //6
            /// \brief  This state will be used when the media player is paused,
            ///         because of a sufficient number of computed buffers
            Paused,             //7
            /// \brief  This state means a clip is mutted and must not be restarted
            Muted,              //8
            /// \brief  An error was encountered, this ClipWorkflow must not be used anymore.
            Error,              //9
        };

        /**
         *  \brief  Used to know which way you want to get a computed output.
         *          Pop: the buffer is popped and returned
         *          Get: the buffer is just returned (for paused mode for instance)
         */
        enum        GetMode
        {
            Pop,
            Get,
        };

        ClipWorkflow( ClipHelper* clip );
        virtual ~ClipWorkflow();

        /**
         *  This method returns the current frame. It locks the renderMutex,
         *  therefore, you can call this method blindly, without taking care
         *  of the rendering process advancement.
         */
        virtual void*           getOutput( ClipWorkflow::GetMode mode ) = 0;
        void                    postGetOutput();
        virtual void            initVlcOutput() = 0;
        void                    initialize();

        /**
         *  \return             true if the ClipWorkflow is able to, and should render
         *                      a frame.
         *
         *  This is true when the state is not stopped, stopping, nor rendering.
         */
        bool                    shouldRender() const;

        /**
         *  Returns the current workflow state.
         *  Be carrefull, as this function is NOT thread safe, and return the
         *  state without locking the state.
         *  It's your job to do it, by calling the getStateLock() method.
         */
        State                   getState() const;

        /**
            \brief              Returns the ClipHelper this workflow instance is based
                                uppon, so that you can query information on it.
            \return             A pointer to a ClipHelper instance.
        */
        inline ClipHelper*      getClipHelper()
        {
            return m_clipHelper;
        }
        inline Clip*            clip()
        {
            return m_clipHelper->clip();
        }

        /**
            \brief  Stop this workflow.
        */
        void                    stop();
        /**
         *  \brief  Set the rendering position
         *  \param  time    The position in millisecond
         */
        virtual void            setTime( qint64 time );

        /**
         *  This method must be used to change the state of the ClipWorkflow
         *  from outside its render loop, otherwise, it may lead to deadlocks.
         *  No additional operations will be executed (for example, if setting
         *  the new state to stop, the media player won't be stopped.)
         *  This is mainly to change the behaviour of the render loop.
         */
        void                    queryStateChange( State newState );

        /**
         *  This returns the QReadWriteLock that protects the ClipWorkflow's state.
         *  It should be use to lock the value when checking states from outside this
         *  class.
         */
        QReadWriteLock*         getStateLock();

        bool                    waitForCompleteInit();

        virtual void*           getLockCallback() const = 0;
        virtual void*           getUnlockCallback() const = 0;

        LibVLCpp::MediaPlayer*  getMediaPlayer();

        /**
         *  \sa MainWorkflow::setFullSpeedRender();
         */
        void                    setFullSpeedRender( bool val );

        void                    mute();
        void                    unmute();

        void                    requireResync();
        /**
         *  \return true if a resync is required.
         *
         *  If a resync is required, true will be returned, and the flag will be
         *  set back to false
         */
        bool                    isResyncRequired();

        /**
         *  \brief              Stop the renderer part of the ClipWorkflow
         *
         *  IE. just stop the VLC thread, and do not flush anything.
         */
        void                    stopRenderer();

        void                    save( QXmlStreamWriter& project ) const;

    private:
        void                    setState( State state );
        void                    adjustBegin();

    protected:
        void                    computePtsDiff( qint64 pts );
        void                    commonUnlock();
        /**
         *  \warning    Must be called from a thread safe context.
         *              This thread safe context has to be set
         *              from the underlying ClipWorkflow implementation.
         */
        virtual quint32         getNbComputedBuffers() const = 0;
        virtual quint32         getMaxComputedBuffers() const = 0;
        /**
         *  \brief  Will empty the computed buffers stack.
         *          This has to be implemented in the underlying
         *          clipworkflow implementation.
         */
        virtual void            flushComputedBuffers() = 0;

        /**
         *  \brief  Release the preallocated buffers
         */
        virtual void            releasePrealocated() = 0;

    private:
        WaitCondition           *m_initWaitCond;
        /**
         *  \brief              Used by the trackworkflow to query a clipworkflow resync.
         *
         *  Basically, this will be used when a clip is moved, and therefore has to be
         *  updated.
         */
        QAtomicInt              m_resyncRequired;

    protected:
        LibVLCpp::MediaPlayer*  m_mediaPlayer;
        ClipHelper*             m_clipHelper;
        QMutex*                 m_renderLock;
        QReadWriteLock*         m_stateLock;
        State                   m_state;
        qint64                  m_previousPts;
        qint64                  m_currentPts;
        /**
         *  \brief  The VLC media used to render
         */
        LibVLCpp::Media*        m_vlcMedia;
        /**
         *  \brief  This is used for basic synchronisation when
         *          the clipworkflow hasn't generate a frame yet,
         *          while the renderer asks for one.
         */
        QWaitCondition          *m_renderWaitCond;
        qint64                  m_beginPausePts;
        qint64                  m_pauseDuration;
        bool                    m_fullSpeedRender;

    private slots:
        void                    loadingComplete();
        void                    clipEndReached();
        void                    mediaPlayerPaused();
        void                    mediaPlayerUnpaused();
        void                    resyncClipWorkflow();
        void                    errorEncountered();

    signals:
        void                    error();
};

#endif // CLIPWORKFLOW_H
