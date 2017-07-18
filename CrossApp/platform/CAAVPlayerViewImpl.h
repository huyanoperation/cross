

#ifndef __CAAVPlayerViewImpl_H__
#define __CAAVPlayerViewImpl_H__

#include <stdio.h>
#include <string>
#include <vector>
#include <functional>
#include "ccMacros.h"
#include "basics/CASize.h"
NS_CC_BEGIN
class CC_DLL CAImage;
class CC_DLL CAAVPlayerView;
class CC_DLL CAAVPlayerViewImpl
{
public:
    
    static CAImage* getFirstFrameImageWithFilePath(const std::string& filePath);
    
    CAAVPlayerViewImpl(CAAVPlayerView* playerView);
    
    virtual ~CAAVPlayerViewImpl();
    
    void setUrl(const std::string& url);
    
    void setFilePath(const std::string& filePath);
    
    void play();
    
    void pause();
    
    void stop();
    
    float getDuration();
    
    float getCurrentTime();
    
    void setCurrentTime(float current);
    
    const DSize& getPresentationSize();
    
private:
    
    void* m_pNativeImpl;
    
    CAAVPlayerView* m_pPlayerView;
};

NS_CC_END

#endif    // __CAAVPlayerViewImpl_H__