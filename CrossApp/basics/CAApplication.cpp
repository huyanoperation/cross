#include <string>
#include "CAApplication.h"
#include "CAFPSImages.h"
#include "view/CADrawingPrimitives.h"
#include "cocoa/CCNS.h"
#include "view/CAWindow.h"
#include "CAScheduler.h"
#include "ccMacros.h"
#include "dispatcher/CATouchDispatcher.h"
#include "support/CAPointExtension.h"
#include "support/CANotificationCenter.h"
#include "images/CAImageCache.h"
#include "basics/CAAutoreleasePool.h"
#include "platform/platform.h"
#include "platform/CAFileUtils.h"
#include "CCApplication.h"
#include "dispatcher/CAKeypadDispatcher.h"
#include "dispatcher/CATouch.h"
#include "support/user_default/CAUserDefault.h"
#include "support/CAProfiling.h"
#include "CCEGLView.h"
#include "platform/CADensityDpi.h"
#include "support/netWork/HttpClient.h"
#include "support/netWork/DownloadManager.h"
#include "game/actions/CGActionManager.h"
#include "support/CAThemeManager.h"
#include "renderer/CCRenderer.h"
#include "renderer/CCGLProgramState.h"
#include "basics/CAConfiguration.h"
#include "basics/CAConsole.h"
#include "renderer/CCFrameBuffer.h"

NS_CC_BEGIN

static CCDisplayLinkDirector *s_pApplication = NULL;

#define kDefaultFPS        60  // 60 frames per second
extern const char* CrossAppVersion(void);

CAApplication* CAApplication::getApplication()
{
    if (!s_pApplication)
    {
        s_pApplication = new CCDisplayLinkDirector();
        s_pApplication->init();
    }

    return s_pApplication;
}

CAApplication::CAApplication(void)
{

}

bool CAApplication::init(void)
{
	setDefaultValues();

    // scenes
    m_pRootWindow = nullptr;

    m_pNotificationView = nullptr;

    // FPS
    m_fAccumDt = 0.0f;
    m_fFrameRate = 0.0f;
    m_pFPSLabel = NULL;
    m_uTotalFrames = m_uFrames = 0;
    m_pszFPS = new char[10];
    m_lastUpdate = std::chrono::steady_clock::now();
    m_fSecondsPerFrame = 1.0f;

    m_nDrawCount = 60;
    m_dAnimationInterval = 1.0 / 60.0f;
    m_bDisplayStats = false;
    m_uNumberOfDraws = 0;
    // paused ?
    m_bPaused = false;
   
    // purge ?
    m_bPurgeApplicationInNextLoop = false;
    m_bRestartApplicationInNextLoop = false;

    m_obWinSizeInPoints = DSizeZero;

    m_pobOpenGLView = nullptr;
    m_defaultFBO = nullptr;
    
    CAPoolManager::getInstance()->push();
    
    m_pConsole = new (std::nothrow) CAConsole;
    
    // scheduler
    m_pScheduler = new (std::nothrow) CAScheduler();
    // action manager
    m_pActionManager = new (std::nothrow) ActionManager();
    m_pScheduler->scheduleSelectorUpdate(m_pActionManager, CAScheduler::PRIORITY_SYSTEM, false);
    
    m_pImageCache = new (std::nothrow) CAImageCache();
    
    this->initMatrixStack();
    m_pRenderer = new (std::nothrow) Renderer;
    RenderState::initialize();
    
    // touchDispatcher
    m_pTouchDispatcher = new CATouchDispatcher();
    m_pTouchDispatcher->init();
    
    // KeypadDispatcher
    m_pKeypadDispatcher = new CAKeypadDispatcher();
    
    // Accelerometer
    m_pAccelerometer = new CAAccelerometer();
    
    m_pThemeManager = nullptr;
    this->setThemeManager(CAThemeManager::create("source_material"));
    
    return true;
}
    
CAApplication::~CAApplication(void)
{
    CC_SAFE_RELEASE(m_pFPSLabel);
    
    CC_SAFE_RELEASE(m_pRootWindow);
    CC_SAFE_RELEASE(m_pNotificationView);
    CC_SAFE_RELEASE(m_pTouchDispatcher);
    CC_SAFE_RELEASE(m_pKeypadDispatcher);
    CC_SAFE_RELEASE(m_pThemeManager);
    CC_SAFE_RELEASE(m_pScheduler);
    CC_SAFE_RELEASE(m_pActionManager);
    CC_SAFE_DELETE(m_pAccelerometer);
    CC_SAFE_DELETE(m_defaultFBO);
    
    // delete fps string
    delete []m_pszFPS;

    CAPoolManager::getInstance()->pop();
    CAPoolManager::destroyInstance();
    CAConfiguration::destroyInstance();
    
    s_pApplication = nullptr;
}

void CAApplication::setDefaultValues(void)
{
	m_dOldAnimationInterval = m_dAnimationInterval = 1.0 / 60;
    m_eProjection = CAApplication::Default;
}

void CAApplication::setGLDefaultValues(void)
{
    // This method SHOULD be called only after openGLView_ was initialized
    CCAssert(m_pobOpenGLView, "opengl view should not be null");

    setAlphaBlending(true);
    setDepthTest(true);
    setProjection(m_eProjection);
}

void CAApplication::updateDraw()
{
    m_nDrawCount = 30;
}

void CAApplication::drawScene(float dt)
{
    //if (m_nDrawCount > 0)
    {
        --m_nDrawCount;
        
        if (m_pRootWindow)
        {
            m_pRootWindow->visitEve();
        }
        
        m_pRenderer->clear();

        experimental::FrameBuffer::clearAllFBOs();
        
        pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        
        m_uNumberOfDraws = 0;
        // draw the scene
        if (m_pRootWindow)
        {
            m_pRenderer->clearDrawStats();
            m_pRootWindow->render(m_pRenderer, Mat4::IDENTITY, nullptr);
        }
        
        // draw the notifications node
        if (m_pNotificationView)
        {
            m_pNotificationView->visit(m_pRenderer, Mat4::IDENTITY, 0);
        }

        if (m_bDisplayStats)
        {
            showStats();
        }
        
        m_pRenderer->render();
        
        popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        
        m_uTotalFrames++;
        
        // swap buffers
        if (m_pobOpenGLView)
        {
            m_pobOpenGLView->swapBuffers();
        }
        
        if (m_bDisplayStats)
        {
            calculateMPF();
        }
    }
}

void CAApplication::calculateDeltaTime(void)
{
    auto now = std::chrono::steady_clock::now();

    if (m_bNextDeltaTimeZero)
    {
        m_fDeltaTime = 0;
        m_bNextDeltaTimeZero = false;
    }
    else
    {
        m_fDeltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastUpdate).count() / 1000000.0f;
        m_fDeltaTime = MAX(0, m_fDeltaTime);
    }

#ifdef CROSSAPP_DEBUG
    // If we are debugging our code, prevent big delta time
    if(m_fDeltaTime > 0.2f)
    {
        m_fDeltaTime = 1 / 60.0f;
    }
#endif

    m_lastUpdate = now;
}

float CAApplication::getDeltaTime()
{
	return m_fDeltaTime;
}

void CAApplication::setOpenGLView(CCEGLView *pobOpenGLView)
{
    CCAssert(pobOpenGLView, "opengl view should not be null");

    if (m_pobOpenGLView != pobOpenGLView)
    {
        CAConfiguration *conf = CAConfiguration::getInstance();
        conf->gatherGPUInfo();
        // EAGLView is not a CAObject
        if(m_pobOpenGLView)
            delete m_pobOpenGLView;
        m_pobOpenGLView = pobOpenGLView;
        
        // set size
        m_obWinSizeInPoints = m_pobOpenGLView->getDesignResolutionSize();
        
        createStatsLabel();
        
        if (m_pobOpenGLView)
        {
            setGLDefaultValues();
        }  
        
        m_pRenderer->initGLView();
        
        CHECK_GL_ERROR_DEBUG();

        m_pobOpenGLView->setTouchDelegate(m_pTouchDispatcher);
        
        m_defaultFBO = experimental::FrameBuffer::getOrCreateDefaultFBO(m_pobOpenGLView);
        m_defaultFBO->retain();
    }
}

void CAApplication::setViewport()
{
    if (m_pobOpenGLView)
    {
        m_pobOpenGLView->setViewPortInPoints(0, 0, m_obWinSizeInPoints.width, m_obWinSizeInPoints.height);
    }
    else
    {
        CACamera::setDefaultViewport(experimental::Viewport(0, 0, s_dip_to_px(m_obWinSizeInPoints.width), s_dip_to_px(m_obWinSizeInPoints.height)));
    }
}

void CAApplication::setNextDeltaTimeZero(bool bNextDeltaTimeZero)
{
    m_bNextDeltaTimeZero = bNextDeltaTimeZero;
}

//
// FIXME TODO
// Matrix code MUST NOT be part of the CAApplication
// MUST BE moved outside.
// Why the CAApplication must have this code ?
//
void CAApplication::initMatrixStack()
{
    while (!_modelViewMatrixStack.empty())
    {
        _modelViewMatrixStack.pop();
    }
    
    while (!_projectionMatrixStack.empty())
    {
        _projectionMatrixStack.pop();
    }
    
    while (!_textureMatrixStack.empty())
    {
        _textureMatrixStack.pop();
    }
    
    _modelViewMatrixStack.push(Mat4::IDENTITY);
    _projectionMatrixStack.push(Mat4::IDENTITY);
    _textureMatrixStack.push(Mat4::IDENTITY);
}

void CAApplication::resetMatrixStack()
{
    initMatrixStack();
}

void CAApplication::popMatrix(MATRIX_STACK_TYPE type)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.pop();
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.pop();
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.pop();
    }
    else
    {
        CCASSERT(false, "unknown matrix stack type");
    }
}

void CAApplication::loadIdentityMatrix(MATRIX_STACK_TYPE type)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.top() = Mat4::IDENTITY;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.top() = Mat4::IDENTITY;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.top() = Mat4::IDENTITY;
    }
    else
    {
        CCASSERT(false, "unknown matrix stack type");
    }
}

void CAApplication::loadMatrix(MATRIX_STACK_TYPE type, const Mat4& mat)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.top() = mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.top() = mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.top() = mat;
    }
    else
    {
        CCASSERT(false, "unknown matrix stack type");
    }
}

void CAApplication::multiplyMatrix(MATRIX_STACK_TYPE type, const Mat4& mat)
{
    if(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW == type)
    {
        _modelViewMatrixStack.top() *= mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION == type)
    {
        _projectionMatrixStack.top() *= mat;
    }
    else if(MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE == type)
    {
        _textureMatrixStack.top() *= mat;
    }
    else
    {
        CCASSERT(false, "unknown matrix stack type");
    }
}

void CAApplication::pushMatrix(MATRIX_STACK_TYPE type)
{
    if(type == MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW)
    {
        _modelViewMatrixStack.push(_modelViewMatrixStack.top());
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION)
    {
        _projectionMatrixStack.push(_projectionMatrixStack.top());
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE)
    {
        _textureMatrixStack.push(_textureMatrixStack.top());
    }
    else
    {
        CCASSERT(false, "unknown matrix stack type");
    }
}

const Mat4& CAApplication::getMatrix(MATRIX_STACK_TYPE type) const
{
    if(type == MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW)
    {
        return _modelViewMatrixStack.top();
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION)
    {
        return _projectionMatrixStack.top();
    }
    else if(type == MATRIX_STACK_TYPE::MATRIX_STACK_TEXTURE)
    {
        return _textureMatrixStack.top();
    }
    
    CCASSERT(false, "unknown matrix stack type, will return modelview matrix instead");
    return  _modelViewMatrixStack.top();
}

void CAApplication::setProjection(CAApplication::Projection kProjection)
{
    DSize size = m_obWinSizeInPoints;

    setViewport();

    switch (kProjection)
    {
    case CAApplication::P2D:
        {
            Mat4 orthoMatrix;
            Mat4::createOrthographicOffCenter(0, size.width, 0, size.height, -1024, 1024, &orthoMatrix);
            loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, orthoMatrix);
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        }
        break;

    case CAApplication::P3D:
        {
            float zeye = this->getZEye();
            
            Mat4 matrixPerspective, matrixLookup;
            
            // issue #1334
            Mat4::createPerspective(60, (GLfloat)size.width/size.height, 10, zeye+size.height/2, &matrixPerspective);
            
            Vec3 eye(size.width/2, size.height/2, zeye), center(size.width/2, size.height/2, 0.0f), up(0.0f, 1.0f, 0.0f);
            Mat4::createLookAt(eye, center, up, &matrixLookup);
            Mat4 proj3d = matrixPerspective * matrixLookup;
            
            loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION, proj3d);
            loadIdentityMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        }
        break;
            
    default:
        CCLOG("CrossApp: CAApplication: unrecognized projection");
        break;
    }

    m_eProjection = kProjection;
    GL::setProjectionMatrixDirty();
}

void CAApplication::purgeCachedData(void)
{
    if (s_pApplication->getOpenGLView())
    {
        CGSpriteFrameCache::getInstance()->removeUnusedSpriteFrames();
        m_pImageCache->removeUnusedImages();
    }
    FileUtils::getInstance()->purgeCachedEntries();
}

float CAApplication::getZEye(void)
{
    return (m_obWinSizeInPoints.height / 1.1565f);
}

void CAApplication::setAlphaBlending(bool bOn)
{
    if (bOn)
    {
        GL::blendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    }
    else
    {
        GL::blendFunc(GL_ONE, GL_ZERO);
    }
    
    CHECK_GL_ERROR_DEBUG();
}

void CAApplication::setDepthTest(bool bOn)
{
    m_pRenderer->setDepthTest(bOn);
}

void CAApplication::setClearColor(const CAColor4F& clearColor)
{
    m_pRenderer->setClearColor(clearColor);
    auto defaultFBO = experimental::FrameBuffer::getOrCreateDefaultFBO(m_pobOpenGLView);
    
    if(defaultFBO) defaultFBO->setClearColor(clearColor);
}

DSize CAApplication::getWinSize(void)
{
    return m_obWinSizeInPoints;
}

DSize CAApplication::getVisibleSize()
{
    if (m_pobOpenGLView)
    {
        return m_pobOpenGLView->getVisibleSize();
    }
    else
    {
        return DSizeZero;
    }
}

DPoint CAApplication::getVisibleOrigin()
{
    if (m_pobOpenGLView)
    {
        return m_pobOpenGLView->getVisibleOrigin();
    }
    else
    {
        return DPointZero;
    }
}

void CAApplication::reshapeProjection(const DSize& newWindowSize)
{
    m_obWinSizeInPoints = DSize(newWindowSize.width, newWindowSize.height);
    setProjection(m_eProjection);
    
    if (m_pRootWindow)
    {
        DRect rect = DRectZero;
        rect.size = newWindowSize;
        m_pRootWindow->setFrame(rect);
        m_pRootWindow->getDefaultCamera()->initDefault();
    }

}

void CAApplication::setStatusBarStyle(const CAStatusBarStyle &var)
{
    m_eStatusBarStyle = var;
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    CCApplication::sharedApplication()->setStatusBarStyle(var);
#endif
    
}

void CAApplication::setStatusBarHidden(bool isStatusBarHidden)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    CCApplication::sharedApplication()->setStatusBarHidden(isStatusBarHidden);
#endif
}

bool CAApplication::isStatusBarHidden()
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    return CCApplication::sharedApplication()->isStatusBarHidden();
#else
    return false;
#endif
}

const CAInterfaceOrientation& CAApplication::getStatusBarOrientation()
{
    return CCEGLView::sharedOpenGLView()->getStatusBarOrientation();
}

void CAApplication::setThemeManager(CAThemeManager* var)
{
    CC_SAFE_RETAIN(var);
    CC_SAFE_RELEASE(m_pThemeManager);
    m_pThemeManager = var;
}


// scene management

void CAApplication::runWindow(CAWindow *pWindow)
{
    if (m_pRootWindow)
    {
        m_pRootWindow->onExitTransitionDidStart();
        m_pRootWindow->onExit();
        m_pRootWindow->release();
        m_pRootWindow = NULL;
    }
    
    CC_SAFE_RETAIN(pWindow);
    m_pRootWindow = pWindow;
    
    startAnimation();
 
    this->run(0);
}

void CAApplication::run(float dt)
{
    if (m_pRootWindow)
    {
        m_pRootWindow->onEnter();
        m_pRootWindow->onEnterTransitionDidFinish();
    }
}

void CAApplication::end()
{
    m_bPurgeApplicationInNextLoop = true;
}

void CAApplication::restart()
{
    m_bRestartApplicationInNextLoop = true;
}

void CAApplication::reset()
{
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    auto sEngine = ScriptEngineManager::getInstance()->getScriptEngine();
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
    
    if (m_pRootWindow)
    {
#if CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        if (sEngine)
        {
            sEngine->releaseScriptObject(this, m_pRootWindow);
        }
#endif // CC_ENABLE_GC_FOR_NATIVE_OBJECTS
        m_pRootWindow->onExitTransitionDidStart();
        m_pRootWindow->onExit();
        m_pRootWindow->release();
    }
    
    m_pRootWindow = nullptr;
    
    // cleanup scheduler
    getScheduler()->unscheduleAll();
    
    this->stopAnimation();
    
    CC_SAFE_RELEASE_NULL(m_pNotificationView);
    CC_SAFE_RELEASE_NULL(m_pFPSLabel);
    // purge all managed caches
    
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (push)
#pragma warning (disable: 4996)
#endif
    //it will crash clang static analyzer so hide it if __clang_analyzer__ defined
#ifndef __clang_analyzer__
    //DrawPrimitives::free();
#endif
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#elif _MSC_VER >= 1400 //vs 2005 or higher
#pragma warning (pop)
#endif
    AnimationCache::destroyInstance();
    CAHttpClient::destroyAllInstance();
    CADownloadManager::destroyInstance();
    GLProgramCache::destroyInstance();
    GLProgramStateCache::destroyInstance();
    FileUtils::destroyInstance();
    
    // cocos2d-x specific data structures
    CAUserDefault::destroyInstance();
    
    GL::invalidateStateCache();
    
    RenderState::finalize();
    
    destroyImageCache();
}

void CAApplication::purgeApplication()
{
    reset();
    
    CHECK_GL_ERROR_DEBUG();
    
    // OpenGL view
    if (m_pobOpenGLView)
    {
        m_pobOpenGLView->end();
        m_pobOpenGLView = nullptr;
    }
    
    // delete CAApplication
    release();
}

void CAApplication::restartApplication()
{
    reset();
    
    // RenderState need to be reinitialized
    RenderState::initialize();
    
    m_pImageCache = new (std::nothrow) CAImageCache();
    
    // Reschedule for action manager
    CAScheduler::getScheduler()->scheduleSelectorUpdate(this->getActionManager(), CAScheduler::PRIORITY_SYSTEM, false);
    
    // release the objects
    CAPoolManager::getInstance()->pop();
    
    // Restart animation
    this->startAnimation();
    
// Real restart in script level
//#if CC_ENABLE_SCRIPT_BINDING
//    ScriptEvent scriptEvent(kRestartGame, nullptr);
//    ScriptEngineManager::getInstance()->getScriptEngine()->sendEvent(&scriptEvent);
//#endif
}

void CAApplication::pause(void)
{
    if (m_bPaused)
    {
        return;
    }

    m_dOldAnimationInterval = m_dAnimationInterval;

    // when paused, don't consume CPU
    setAnimationInterval(1 / 4.0);
    m_bPaused = true;
}

void CAApplication::resume(void)
{
    if (! m_bPaused)
    {
        return;
    }

    setAnimationInterval(m_dOldAnimationInterval);

    m_bPaused = false;
    m_fDeltaTime = 0;
    
    setNextDeltaTimeZero(true);
}

// display the FPS using a LabelAtlas
// updates the FPS every frame
void CAApplication::showStats(void)
{
    m_uFrames++;
    m_fAccumDt += m_fDeltaTime;
    
    if (m_bDisplayStats && m_pFPSLabel)
    {
        if (m_fAccumDt > CC_DIRECTOR_STATS_INTERVAL)
        {
            m_fFrameRate = m_uFrames / m_fAccumDt;
            m_uFrames = 0;
            m_fAccumDt = 0;
            
            sprintf(m_pszFPS, "%.1f", m_fFrameRate);
            m_pFPSLabel->setText(m_pszFPS);
        }
        m_pFPSLabel->visitEve();
        m_pFPSLabel->visit();
    }
}

void CAApplication::calculateMPF()
{
    static float prevSecondsPerFrame = 0;
    static const float MPF_FILTER = 0.10f;
    
    auto now = std::chrono::steady_clock::now();
    
    m_fSecondsPerFrame = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastUpdate).count() / 1000000.0f;
    
    m_fSecondsPerFrame = m_fSecondsPerFrame * MPF_FILTER + (1-MPF_FILTER) * prevSecondsPerFrame;
    prevSecondsPerFrame = m_fSecondsPerFrame;
}

// returns the FPS image data pointer and len
void CAApplication::getFPSImageData(unsigned char** datapointer, unsigned int* length)
{
    // XXX fixed me if it should be used 
    *datapointer = cc_fps_images_png;
	*length = cc_fps_images_len();
}

void CAApplication::createStatsLabel()
{
    if( m_pFPSLabel)
    {
        CC_SAFE_RELEASE_NULL(m_pFPSLabel);
        //CAApplication::getApplication()->getImageCache()->removeImageForKey("cc_fps_images");
        FileUtils::getInstance()->purgeCachedEntries();
    }

//    unsigned char *data = NULL;
//    unsigned int data_len = 0;
//    getFPSImageData(&data, &data_len);
//
//    CAImage* image = CAImage::createWithImageData(data, data_len, "cc_fps_images");
    
    m_pFPSLabel = CALabel::createWithFrame(DRect(20, 20, 100, 50));
    m_pFPSLabel->setFontSize(40);
	m_pFPSLabel->setColor(CAColor_yellow);
    CC_SAFE_RETAIN(m_pFPSLabel);
    m_pFPSLabel->onEnter();
    m_pFPSLabel->onEnterTransitionDidFinish();
}

CAView* CAApplication::getNotificationView()
{ 
    return m_pNotificationView; 
}

void CAApplication::setNotificationView(CAView *view)
{
    if (m_pNotificationView)
    {
        m_pNotificationView->resignFirstResponder();
    }
    CC_SAFE_RELEASE(m_pNotificationView);
    m_pNotificationView = view;
    if (m_pNotificationView)
    {
        m_pNotificationView->becomeFirstResponder();
    }
    CC_SAFE_RETAIN(m_pNotificationView);
    this->updateDraw();
}

void CAApplication::setScheduler(CAScheduler* scheduler)
{
    if (m_pScheduler != scheduler)
    {
        CC_SAFE_RETAIN(scheduler);
        CC_SAFE_RELEASE(m_pScheduler);
        m_pScheduler = scheduler;
    }
}

void CAApplication::setActionManager(ActionManager* actionManager)
{
    if (m_pActionManager != actionManager)
    {
        CC_SAFE_RETAIN(actionManager);
        CC_SAFE_RELEASE(m_pActionManager);
        m_pActionManager = actionManager;
    }
}

void CAApplication::setImageCache(CAImageCache *imageCache)
{
    if (m_pImageCache != imageCache)
    {
        CC_SAFE_RETAIN(imageCache);
        CC_SAFE_RELEASE(m_pImageCache);
        m_pImageCache = imageCache;
    }
}

void CAApplication::destroyImageCache()
{
    if (m_pImageCache)
    {
        //m_pImageCache->waitForQuit();
        CC_SAFE_RELEASE_NULL(m_pImageCache);
    }
}

void CAApplication::setTouchDispatcher(CATouchDispatcher* pTouchDispatcher)
{
    if (m_pTouchDispatcher != pTouchDispatcher)
    {
        CC_SAFE_RETAIN(pTouchDispatcher);
        CC_SAFE_RELEASE(m_pTouchDispatcher);
        m_pTouchDispatcher = pTouchDispatcher;
    }    
}

CATouchDispatcher* CAApplication::getTouchDispatcher()
{
    return m_pTouchDispatcher;
}

void CAApplication::setKeypadDispatcher(CAKeypadDispatcher* pKeypadDispatcher)
{
    CC_SAFE_RETAIN(pKeypadDispatcher);
    CC_SAFE_RELEASE(m_pKeypadDispatcher);
    m_pKeypadDispatcher = pKeypadDispatcher;
}

CAKeypadDispatcher* CAApplication::getKeypadDispatcher()
{
    return m_pKeypadDispatcher;
}

void CAApplication::setAccelerometer(CAAccelerometer* pAccelerometer)
{
    if (m_pAccelerometer != pAccelerometer)
    {
        CC_SAFE_DELETE(m_pAccelerometer);
        m_pAccelerometer = pAccelerometer;
    }
}

CAAccelerometer* CAApplication::getAccelerometer()
{
    return m_pAccelerometer;
}

unsigned long CAApplication::getCurrentNumberOfDraws()
{
    return ++m_uNumberOfDraws;
}

/***************************************************
* implementation of DisplayLinkDirector
**************************************************/

// should we implement 4 types of director ??
// I think DisplayLinkDirector is enough
// so we now only support DisplayLinkDirector
void CCDisplayLinkDirector::startAnimation(void)
{
    m_lastUpdate = std::chrono::steady_clock::now();

    m_bInvalid = false;
    m_nDrawCount = 180;
    
    m_uCrossAppThreadID = std::this_thread::get_id();
    
    CCApplication::sharedApplication()->setAnimationInterval(m_dAnimationInterval);
    
    // fix issue #3509, skip one fps to avoid incorrect time calculation.
    setNextDeltaTimeZero(true);
}

void CCDisplayLinkDirector::mainLoop(void)
{
    // calculate "global" dt
   
    
    if (m_bPurgeApplicationInNextLoop)
    {
        m_bPurgeApplicationInNextLoop = false;
        purgeApplication();
    }
    else if (m_bRestartApplicationInNextLoop)
    {
        m_bRestartApplicationInNextLoop = false;
        restartApplication();
    }
    else if (! m_bInvalid)
    {
        calculateDeltaTime();
        
        if (! m_bPaused)
        {
            CAScheduler::getScheduler()->update(m_fDeltaTime);
            CAObject::updateDelayTimers(m_fDeltaTime);
            drawScene();
        }
        CAPoolManager::getInstance()->pop();}
    }


void CCDisplayLinkDirector::stopAnimation(void)
{
    m_bInvalid = true;
}

void CCDisplayLinkDirector::setAnimationInterval(double dValue)
{
    m_dAnimationInterval = dValue;
    if (! m_bInvalid)
    {
        stopAnimation();
        startAnimation();
    }    
}

NS_CC_END

