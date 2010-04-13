/*****************************************************************************
 * MainWindow.cpp: VLMC MainWindow
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *          Hugo Beauzee-Luyssen <beauze.h@gmail.com>
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

#include <QSizePolicy>
#include <QDockWidget>
#include <QFileDialog>
#include <QSlider>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>

#include "MainWindow.h"
#include "config.h"
#include "Library.h"
#include "About.h"
#include "VlmcDebug.h"

#include "MainWorkflow.h"
#include "export/RendererSettings.h"
#include "WorkflowFileRenderer.h"
#include "WorkflowRenderer.h"
#include "ClipRenderer.h"
#include "EffectsEngine.h"

/* Widgets */
#include "DockWidgetManager.h"
#include "ImportController.h"
#include "MediaListView.h"
#include "PreviewWidget.h"
#include "timeline/Timeline.h"
#include "timeline/TracksView.h"
#include "UndoStack.h"

/* Settings / Preferences */
#include "project/GuiProjectManager.h"
#include "ProjectWizard.h"
#include "Settings.h"
#include "SettingsManager.h"
#include "LanguageHelper.h"

/* VLCpp */
#include "VLCInstance.h"

MainWindow::MainWindow( QWidget *parent ) :
    QMainWindow( parent ), m_fileRenderer( NULL )
{
    m_ui.setupUi( this );

    qRegisterMetaType<MainWorkflow::TrackType>( "MainWorkflow::TrackType" );
    qRegisterMetaType<MainWorkflow::FrameChangedReason>( "MainWorkflow::FrameChangedReason" );
    qRegisterMetaType<QVariant>( "QVariant" );

    //We only install message handler here cause it uses configuration.
//    VlmcDebug::getInstance()->setup();

    //VLC Instance:
    LibVLCpp::Instance::getInstance();

    //Creating the project manager first (so it can create all the project variables)
    GUIProjectManager::getInstance();

    //Preferences
    initVlmcPreferences();

    // GUI
    DockWidgetManager::getInstance( this )->setMainWindow( this );
    createGlobalPreferences();
    createProjectPreferences();
    initializeDockWidgets();
    createStatusBar();
#ifdef WITH_CRASHBUTTON
    setupCrashTester();
#endif

    // Zoom
    connect( m_zoomSlider, SIGNAL( valueChanged( int ) ),
             m_timeline, SLOT( changeZoom( int ) ) );
    connect( m_timeline->tracksView(), SIGNAL( zoomIn() ),
             this, SLOT( zoomIn() ) );
    connect( m_timeline->tracksView(), SIGNAL( zoomOut() ),
             this, SLOT( zoomOut() ) );
    connect( this, SIGNAL( toolChanged( ToolButtons ) ),
             m_timeline, SLOT( setTool( ToolButtons ) ) );

    connect( GUIProjectManager::getInstance(), SIGNAL( projectUpdated( const QString&, bool ) ),
             this, SLOT( projectUpdated( const QString&, bool ) ) );

    // Undo/Redo
    connect( UndoStack::getInstance( this ), SIGNAL( canRedoChanged( bool ) ),
             this, SLOT( canRedoChanged( bool ) ) );
    connect( UndoStack::getInstance( this ), SIGNAL( canUndoChanged( bool ) ),
             this, SLOT( canUndoChanged( bool ) ) );
    canRedoChanged( UndoStack::getInstance( this )->canRedo() );
    canUndoChanged( UndoStack::getInstance( this )->canUndo() );


    // Wizard
    m_pWizard = new ProjectWizard( this );
    m_pWizard->setModal( true );


#ifdef WITH_CRASHHANDLER
    if ( restoreSession() == true )
        return ;
#endif
    QSettings s;

    if ( s.value( "ShowWizardStartup", true ).toBool() )
    {
        //If a project was opened from the command line: don't show the wizzard.
        if ( qApp->arguments().size() == 1 )
            m_pWizard->show();
    }
}

MainWindow::~MainWindow()
{
    QSettings s;
    // Save the current geometry
    s.setValue( "MainWindowGeometry", saveGeometry() );
    // Save the current layout
    s.setValue( "MainWindowState", saveState() );
    s.setValue( "CleanQuit", true );
    s.sync();

    if ( m_fileRenderer )
        delete m_fileRenderer;
    delete m_importController;
    LibVLCpp::Instance::destroyInstance();
}

void MainWindow::changeEvent( QEvent *e )
{
    switch ( e->type() )
    {
    case QEvent::LanguageChange:
        m_ui.retranslateUi( this );
        break;
    default:
        break;
    }
}

//use this helper when the shortcut is binded to a menu action
#define CREATE_MENU_SHORTCUT( key, defaultValue, name, desc, actionInstance  )       \
        VLMC_CREATE_PREFERENCE_KEYBOARD( key, defaultValue, name, desc );    \
        KeyboardShortcutHelper  *helper##actionInstance = new KeyboardShortcutHelper( key, m_ui.actionInstance, this );

void
MainWindow::initVlmcPreferences()
{

    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/defaultmode", "n", "Select mode", "Select the selection tool in the timeline" );
    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/cutmode", "x", "Cut mode", "Select the cut/razor tool in the timeline" );
    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/mediapreview", "Ctrl+Return", "Media preview", "Preview the selected media, or pause the current preview" );
    VLMC_CREATE_PREFERENCE_KEYBOARD( "keyboard/renderpreview", "Space", "Render preview", "Preview the project, or pause the current preview" );
    //A bit nasty, but we better use what Qt's providing as default shortcut
    CREATE_MENU_SHORTCUT( "keyboard/undo", QKeySequence( QKeySequence::Undo ).toString().toLocal8Bit(), "Undo", "Undo the last action", actionUndo );
    CREATE_MENU_SHORTCUT( "keyboard/redo", QKeySequence( QKeySequence::Redo ).toString().toLocal8Bit(), "Redo", "Redo the last action", actionRedo );
    CREATE_MENU_SHORTCUT( "keyboard/help", QKeySequence( QKeySequence::HelpContents ).toString().toLocal8Bit(), "Help", "Toggle the help page", actionHelp );
    CREATE_MENU_SHORTCUT( "keyboard/quit", "Ctrl+Q", "Quit", "Quit VLMC", actionQuit );
    CREATE_MENU_SHORTCUT( "keyboard/preferences", "Alt+P", "Preferences", "Open VLMC preferences", actionPreferences );
    CREATE_MENU_SHORTCUT( "keyboard/fullscreen", "F", "Fullscreen", "Switch to fullscreen mode", actionFullscreen );
    CREATE_MENU_SHORTCUT( "keyboard/newproject", QKeySequence( QKeySequence::New ).toString().toLocal8Bit(), "New project", "Open the new project wizzard", actionNew_Project );
    CREATE_MENU_SHORTCUT( "keyboard/openproject", QKeySequence( QKeySequence::Open ).toString().toLocal8Bit(), "Open a project", "Open an existing project", actionLoad_Project );
    CREATE_MENU_SHORTCUT( "keyboard/save", QKeySequence( QKeySequence::Save ).toString().toLocal8Bit(), "Save", "Save the current project", actionSave );
    CREATE_MENU_SHORTCUT( "keyboard/saveas", "Ctrl+Shift+S", "Save as", "Save the current project to a new file", actionSave_As );
    CREATE_MENU_SHORTCUT( "keyboard/closeproject", QKeySequence( QKeySequence::Close ).toString().toLocal8Bit(), "Close the project", "Close the current project", actionClose_Project );
    CREATE_MENU_SHORTCUT( "keyboard/importmedia", "Ctrl+I", "Import media", "Open the import window", actionImport );
    CREATE_MENU_SHORTCUT( "keyboard/renderproject", "Ctrl+R", "Render the project", "Render the project to a file", actionRender );

    VLMC_CREATE_PREFERENCE_LANGUAGE( "general/VLMCLang", "default", "Langage", "The VLMC's UI language" );
    SettingsManager::getInstance()->watchValue( "general/VLMCLang",
                                                LanguageHelper::getInstance(),
                                                SLOT( languageChanged( const QVariant& ) ),
                                                SettingsManager::Vlmc );

    //Load saved preferences :
    QSettings       s;
    if ( s.value( "VlmcVersion" ).toString() != PROJECT_VERSION )
        s.clear();
    else
    {
        loadVlmcPreferences( "keyboard" );
        loadVlmcPreferences( "general" );
    }
    s.setValue( "VlmcVersion", PROJECT_VERSION );
}

void
MainWindow::loadVlmcPreferences( const QString &subPart )
{
    QSettings       s;
    s.beginGroup( subPart );
    foreach ( QString key, s.allKeys() )
    {
        SettingsManager::getInstance()->setValue( subPart + "/" + key, s.value( key ),
                                                  SettingsManager::Vlmc );
    }
}

#undef CREATE_MENU_SHORTCUT

void
MainWindow::setupLibrary()
{
    //GUI part :
    QWidget     *libraryWidget = new QWidget( this );
    libraryWidget->setMinimumWidth( 280 );

    QPushButton *button = new QPushButton( tr( "Import" ), this );
    connect( button, SIGNAL( clicked() ), this, SLOT( on_actionImport_triggered() ) );

    StackViewController *nav = new StackViewController( libraryWidget );
    MediaListView   *mediaView = new MediaListView( nav, Library::getInstance() );
    nav->pushViewController( mediaView );
    libraryWidget->layout()->addWidget( button );

    m_importController = new ImportController();
    const ClipRenderer* clipRenderer = qobject_cast<const ClipRenderer*>( m_clipPreview->getGenericRenderer() );
    Q_ASSERT( clipRenderer != NULL );

    DockWidgetManager::getInstance()->addDockedWidget( libraryWidget, tr( "Media Library" ),
                                                    Qt::AllDockWidgetAreas,
                                                    QDockWidget::AllDockWidgetFeatures,
                                                    Qt::LeftDockWidgetArea );
    connect( mediaView, SIGNAL( clipSelected( Clip* ) ),
             clipRenderer, SLOT( setClip( Clip* ) ) );

    connect( Library::getInstance(), SIGNAL( clipRemoved( const QUuid& ) ),
             clipRenderer, SLOT( clipUnloaded( const QUuid& ) ) );
}

void    MainWindow::on_actionSave_triggered()
{
    GUIProjectManager::getInstance()->saveProject();
}

void    MainWindow::on_actionSave_As_triggered()
{
    GUIProjectManager::getInstance()->saveProject( true );
}

void    MainWindow::on_actionLoad_Project_triggered()
{
    GUIProjectManager* pm = GUIProjectManager::getInstance();
    pm->loadProject( pm->acquireProjectFileName() );
}

void MainWindow::createStatusBar()
{
    // Mouse (default) tool
    QToolButton* mouseTool = new QToolButton( this );
    mouseTool->setAutoRaise( true );
    mouseTool->setCheckable( true );
    mouseTool->setIcon( QIcon( ":/images/mouse" ) );
    m_ui.statusbar->addPermanentWidget( mouseTool );

    // Cut/Split tool
    QToolButton* splitTool = new QToolButton( this );
    splitTool->setAutoRaise( true );
    splitTool->setCheckable( true );
    splitTool->setIcon( QIcon( ":/images/editcut" ) );
    m_ui.statusbar->addPermanentWidget( splitTool );

    // Group the two previous buttons
    QButtonGroup* toolButtonGroup = new QButtonGroup( this );
    toolButtonGroup->addButton( mouseTool, TOOL_DEFAULT);
    toolButtonGroup->addButton( splitTool, TOOL_CUT );
    toolButtonGroup->setExclusive( true );
    mouseTool->setChecked( true );

    //Shortcut part:
    KeyboardShortcutHelper* defaultModeShortcut = new KeyboardShortcutHelper( "keyboard/defaultmode", this );
    KeyboardShortcutHelper* cutModeShortcut = new KeyboardShortcutHelper( "keyboard/cutmode", this );
    connect( defaultModeShortcut, SIGNAL( activated() ), mouseTool, SLOT( click() ) );
    connect( cutModeShortcut, SIGNAL( activated() ), splitTool, SLOT( click() ) );

    connect( toolButtonGroup, SIGNAL( buttonClicked( int ) ),
             this, SLOT( toolButtonClicked( int ) ) );

    // Spacer
    QWidget* spacer = new QWidget( this );
    spacer->setFixedWidth( 20 );
    m_ui.statusbar->addPermanentWidget( spacer );

    // Zoom Out
    QToolButton* zoomOutButton = new QToolButton( this );
    zoomOutButton->setIcon( QIcon( ":/images/zoomout" ) );
    m_ui.statusbar->addPermanentWidget( zoomOutButton );
    connect( zoomOutButton, SIGNAL( clicked() ),
             this, SLOT( zoomOut() ) );

    // Zoom slider
    m_zoomSlider = new QSlider( this );
    m_zoomSlider->setOrientation( Qt::Horizontal );
    m_zoomSlider->setTickInterval( 1 );
    m_zoomSlider->setSingleStep( 1 );
    m_zoomSlider->setPageStep( 1 );
    m_zoomSlider->setMinimum( 0 );
    m_zoomSlider->setMaximum( 13 );
    m_zoomSlider->setValue( 10 );
    m_zoomSlider->setFixedWidth( 80 );
    m_zoomSlider->setInvertedAppearance( true );
    m_ui.statusbar->addPermanentWidget( m_zoomSlider );

    // Zoom IN
    QToolButton* zoomInButton = new QToolButton( this );
    zoomInButton->setIcon( QIcon( ":/images/zoomin" ) );
    m_ui.statusbar->addPermanentWidget( zoomInButton );
    connect( zoomInButton, SIGNAL( clicked() ),
             this, SLOT( zoomIn() ) );
}

void MainWindow::initializeDockWidgets( void )
{
    m_renderer = new WorkflowRenderer();
    m_renderer->initializeRenderer();
    m_timeline = new Timeline( m_renderer, this );
    m_timeline->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    m_timeline->show();
    setCentralWidget( m_timeline );

    DockWidgetManager *dockManager = DockWidgetManager::getInstance();

    m_clipPreview = new PreviewWidget( new ClipRenderer, this );
    dockManager->addDockedWidget( m_clipPreview,
                                  tr( "Clip Preview" ),
                                  Qt::AllDockWidgetAreas,
                                  QDockWidget::AllDockWidgetFeatures,
                                  Qt::TopDockWidgetArea );
    KeyboardShortcutHelper* clipShortcut = new KeyboardShortcutHelper( "keyboard/mediapreview", this );
    connect( clipShortcut, SIGNAL( activated() ), m_clipPreview, SLOT( on_pushButtonPlay_clicked() ) );

    m_projectPreview = new PreviewWidget( m_renderer, this );
    dockManager->addDockedWidget( m_projectPreview,
                                  tr( "Project Preview" ),
                                  Qt::AllDockWidgetAreas,
                                  QDockWidget::AllDockWidgetFeatures,
                                  Qt::TopDockWidgetArea );
    KeyboardShortcutHelper* renderShortcut = new KeyboardShortcutHelper( "keyboard/renderpreview", this );
    connect( renderShortcut, SIGNAL( activated() ), m_projectPreview, SLOT( on_pushButtonPlay_clicked() ) );

    QDockWidget* dock = dockManager->addDockedWidget( UndoStack::getInstance( this ),
                                  tr( "History" ),
                                  Qt::AllDockWidgetAreas,
                                  QDockWidget::AllDockWidgetFeatures,
                                  Qt::LeftDockWidgetArea );
    if ( dock != 0 )
        dock->hide();
    setupLibrary();
}

void        MainWindow::createGlobalPreferences()
{
    m_globalPreferences = new Settings( SettingsManager::Vlmc, this );
    m_globalPreferences->addCategorie( "general", SettingsManager::Vlmc,
                                   QIcon( ":/images/images/vlmc.png" ),
                                   tr ( "VLMC settings" ) );
    m_globalPreferences->addCategorie( "keyboard", SettingsManager::Vlmc,
                                     QIcon( ":/images/keyboard" ),
                                     tr( "Keyboard Settings" ) );
}

void    MainWindow::createProjectPreferences()
{
    m_projectPreferences = new Settings( SettingsManager::Project, this );
    m_projectPreferences->addCategorie( "general", SettingsManager::Project,
                                   QIcon( ":/images/images/vlmc.png" ),
                                   tr ( "Project settings" ) );
    m_projectPreferences->addCategorie( "video", SettingsManager::Project,
                                   QIcon( ":/images/images/video.png" ),
                                   tr ( "Video settings" ) );
    m_projectPreferences->addCategorie( "audio", SettingsManager::Project,
                                   QIcon( ":/images/images/audio.png" ),
                                   tr ( "Audio settings" ) );
}


//Private slots definition

void MainWindow::on_actionQuit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_actionPreferences_triggered()
{
   m_globalPreferences->show();
}

void MainWindow::on_actionAbout_triggered()
{
    About::getInstance()->exec();
}

void MainWindow::on_actionTranscode_triggered()
{
    QMessageBox::information( this, tr( "Sorry" ),
                              tr( "This feature is currently disabled." ) );
    //Transcode::instance( this )->exec();
}

void    MainWindow::on_actionRender_triggered()
{
    if ( MainWorkflow::getInstance()->getLengthFrame() <= 0 )
    {
        QMessageBox::warning( NULL, tr ( "VLMC Renderer" ), tr( "There is nothing to render." ) );
        return ;
    }
    else
    {
        m_renderer->killRenderer();
        //Setup dialog box for querying render parameters.
        RendererSettings    *settings = new RendererSettings;
        if ( settings->exec() == QDialog::Rejected )
        {
            delete settings;
            return ;
        }
        QString     outputFileName = settings->outputFileName();
        quint32     width = settings->width();
        quint32     height = settings->height();
        double      fps = settings->fps();
        quint32     vbitrate = settings->videoBitrate();
        quint32     abitrate = settings->audioBitrate();
        delete settings;

        if ( m_fileRenderer )
            delete m_fileRenderer;
        m_fileRenderer = new WorkflowFileRenderer();
        WorkflowFileRendererDialog  *dialog = new WorkflowFileRendererDialog( m_fileRenderer, width, height );
        dialog->setModal( true );
        dialog->setOutputFileName( outputFileName );

        m_fileRenderer->initializeRenderer();
        m_fileRenderer->run( outputFileName, width, height, fps, vbitrate, abitrate );
        dialog->exec();
        delete dialog;
    }
}

void MainWindow::on_actionNew_Project_triggered()
{
    m_pWizard->restart();
    m_pWizard->show();
}

void    MainWindow::on_actionHelp_triggered()
{
    QDesktopServices::openUrl( QUrl( "http://vlmc.org" ) );
}

void    MainWindow::zoomIn()
{
    m_zoomSlider->setValue( m_zoomSlider->value() - 1 );
}

void    MainWindow::zoomOut()
{
    m_zoomSlider->setValue( m_zoomSlider->value() + 1 );
}

void    MainWindow::on_actionFullscreen_triggered( bool checked )
{
    if ( checked )
        showFullScreen();
    else
        showNormal();
}

void    MainWindow::registerWidgetInWindowMenu( QDockWidget* widget )
{
    m_ui.menuWindow->addAction( widget->toggleViewAction() );
}

void    MainWindow::toolButtonClicked( int id )
{
    emit toolChanged( (ToolButtons)id );
}

void MainWindow::on_actionBypass_effects_engine_toggled(bool)
{
//    EffectsEngine*  ee;
//
//    ee = MainWorkflow::getInstance()->getEffectsEngine();
//    if (toggled)
//        ee->enable();
//    else
//       ee->disable();
    return ;
}

void MainWindow::on_actionProject_Preferences_triggered()
{
  m_projectPreferences->show();
}

void    MainWindow::closeEvent( QCloseEvent* e )
{
    GUIProjectManager   *pm = GUIProjectManager::getInstance();
    if ( pm->askForSaveIfModified() )
        e->accept();
    else
        e->ignore();
}

void    MainWindow::projectUpdated( const QString& projectName, bool savedStatus )
{
    QString title = tr( "VideoLAN Movie Creator" );
    title += " - ";
    title += projectName;
    if ( savedStatus == false )
        title += " *";
    setWindowTitle( title );
}

void    MainWindow::on_actionClose_Project_triggered()
{
    GUIProjectManager::getInstance()->closeProject();
}

void    MainWindow::on_actionUndo_triggered()
{
    UndoStack::getInstance( this )->undo();
}

void    MainWindow::on_actionRedo_triggered()
{
    UndoStack::getInstance( this )->redo();
}

void    MainWindow::on_actionCrash_triggered()
{
    //WARNING: read this part at your own risk !!
    //I'm not responsible if you puke while reading this :D
    QString str;
    int test = 1 / str.length();
    Q_UNUSED( test );
}

bool    MainWindow::restoreSession()
{
    QSettings   s;
    bool        fileCreated = false;
    bool        ret = false;

    fileCreated = s.contains( "VlmcVersion" );
    if ( fileCreated == true )
    {
        bool        cleanQuit = s.value( "CleanQuit", true ).toBool();

        // Restore the geometry
        restoreGeometry( s.value( "MainWindowGeometry" ).toByteArray() );
        // Restore the layout
        restoreState( s.value( "MainWindowState" ).toByteArray() );

        if ( cleanQuit == false )
        {
            QMessageBox::StandardButton res = QMessageBox::question( this, tr( "Crash recovery" ), tr( "VLMC didn't closed nicely. Do you wan't to recover your project ?" ),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
            if ( res == QMessageBox::Yes )
            {
                if ( GUIProjectManager::getInstance()->loadEmergencyBackup() == true )
                    ret = true;
                else
                    QMessageBox::warning( this, tr( "Can't restore project" ), tr( "VLMC didn't manage to restore your project. We appology for the inconvenience" ) );
            }
        }
    }
    s.setValue( "CleanQuit", false );
    s.sync();
    return ret;
}

void    MainWindow::on_actionImport_triggered()
{
    m_importController->exec();
}

void    MainWindow::canUndoChanged( bool canUndo )
{
    m_ui.actionUndo->setEnabled( canUndo );
}

void    MainWindow::canRedoChanged( bool canRedo )
{
    m_ui.actionRedo->setEnabled( canRedo );
}

#ifdef WITH_CRASHBUTTON
void    MainWindow::setupCrashTester()
{
    QAction* actionCrash = new QAction( this );
    actionCrash->setObjectName( QString::fromUtf8( "actionCrash" ) );
    m_ui.menuTools->addAction( actionCrash );
    actionCrash->setText( QApplication::translate( "MainWindow", "Crash", 0, QApplication::UnicodeUTF8 ) );
    connect( actionCrash, SIGNAL( triggered( bool ) ), this, SLOT( on_actionCrash_triggered() ) );
}
#endif

