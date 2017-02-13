//
//  CASlider.m
//  CrossApp 
//
//  Created by juguanhui on 14-5-30.
//  Copyright (c) 2014年 CrossApp-x. All rights reserved.
//

#include "CASlider.h"
#include "view/CAImageView.h"
#include "dispatcher/CATouch.h"
#include "view/CAScale9ImageView.h"
#include "support/CAPointExtension.h"
#include "view/CARenderImage.h"
#include "basics/CAApplication.h"
#include "support/CAThemeManager.h"
#include "support/ccUtils.h"
NS_CC_BEGIN

CASlider::CASlider()
    : CAControl()
    , m_bTouchClick(false)
    , m_fValue(0.0f)
    , m_fMinValue(0.0f)
    , m_fMaxValue(1.0f)
    , m_fTrackHeight(4.0f)
    , m_pMinTrackTintImage(NULL)
    , m_pMaxTrackTintImage(NULL)
    , m_pThumbTintImage(NULL)
    , m_pMinTrackTintImageView(NULL)
    , m_pMaxTrackTintImageView(NULL)
    , m_pThumbTintImageView(NULL)
{
    this->setPriorityScroll(true);
    this->setVerticalScrollEnabled(false);
    const CAThemeManager::stringMap& map = CAApplication::getApplication()->getThemeManager()->getThemeMap("CASlider");
    int h = atoi(map.at("trackHeight").c_str());
    m_fTrackHeight = (h == 0) ? m_fTrackHeight : h;
}

CASlider::~CASlider()
{
    CC_SAFE_RELEASE_NULL(m_pMinTrackTintImage);
	CC_SAFE_RELEASE_NULL(m_pMaxTrackTintImage);
	CC_SAFE_RELEASE_NULL(m_pThumbTintImage);
}

void CASlider::onExitTransitionDidStart()
{
    CAControl::onExitTransitionDidStart();
}

void CASlider::onEnterTransitionDidFinish()
{
    CAControl::onEnterTransitionDidFinish();
}

CASlider* CASlider::create()
{
    CASlider* slider = new CASlider();
    
    if (slider && slider->init())
    {
        slider->autorelease();
        return slider;
    }
    
    CC_SAFE_DELETE(slider);
    return NULL;
}

CASlider* CASlider::createWithFrame(const DRect& rect)
{
    CASlider* slider = new CASlider();
    
    if (slider && slider->initWithFrame(rect))
    {
        slider->autorelease();
        return slider;
    }
    
    CC_SAFE_DELETE(slider);
    return NULL;
}

CASlider* CASlider::createWithCenter(const DRect& rect)
{
    CASlider* slider = new CASlider();
    
    if (slider && slider->initWithCenter(rect))
    {
        slider->autorelease();
        return slider;
    }
    
    CC_SAFE_DELETE(slider);
    return NULL;
}

CASlider* CASlider::createWithLayout(const CrossApp::DLayout &layout)
{
    CASlider* slider = new CASlider();
    
    if (slider && slider->initWithLayout(layout))
    {
        slider->autorelease();
        return slider;
    }
    
    CC_SAFE_DELETE(slider);
    return NULL;
}

bool CASlider::init()
{
    if (!CAControl::init())
    {
        return false;
    }
    const CAThemeManager::stringMap& map = CAApplication::getApplication()->getThemeManager()->getThemeMap("CASlider");
    m_pMinTrackTintImageView = CAScale9ImageView::create();
    this->addSubview(m_pMinTrackTintImageView);
    this->setMinTrackTintImage(CAImage::create(map.at("minTrackTintImage")));
    
    
    m_pMaxTrackTintImageView = CAScale9ImageView::create();
    this->addSubview(m_pMaxTrackTintImageView);
    this->setMaxTrackTintImage(CAImage::create(map.at("maxTrackTintImage")));
    
    
    m_pThumbTintImageView = CAImageView::create();
    this->addSubview(m_pThumbTintImageView);
    this->setThumbTintImage(CAImage::create(map.at("thumbTintImage")));
    
    return true;
}

bool CASlider::initWithFrame(const DRect& rect)
{
    if (!this->init())
    {
        return false;
    }

    this->setFrame(rect);
    return true;
}

bool CASlider::initWithCenter(const DRect& rect)
{
    if (!this->init())
    {
        return false;
    }

    this->setCenter(rect);
    return true;
}

void CASlider::layoutSubViews()
{
    if (m_fValue >= m_fMinValue
        && m_fValue <= m_fMaxValue
        && m_pThumbTintImageView
        && m_pMinTrackTintImageView
        && m_pMaxTrackTintImageView)
    {
        float thumbDiameter = m_obContentSize.height;
        float totalWidth = m_obContentSize.width;
        float percent = (m_fValue - m_fMinValue) / (m_fMaxValue - m_fMinValue);
        float centerX = ((totalWidth - thumbDiameter) * percent) + thumbDiameter/2;
        
        m_pThumbTintImageView->setCenter(DRect(centerX, thumbDiameter / 2, thumbDiameter, thumbDiameter));
        m_pMinTrackTintImageView->setLayout(DLayout(DHorizontalLayout_L_W(0, centerX), DVerticalLayout_H_C(m_fTrackHeight, 0.5f)));
        m_pMaxTrackTintImageView->setLayout(DLayout(DHorizontalLayout_L_R(centerX, 0), DVerticalLayout_H_C(m_fTrackHeight, 0.5f)));
    }
}

void CASlider::setValue(float value)
{
    if (m_fValue != value)
    {
        m_fValue = value;
        m_fValue = MAX(m_fValue, m_fMinValue);
        m_fValue = MIN(m_fValue, m_fMaxValue);
        this->layoutSubViews();
    }
}

void CASlider::setMinValue(float minValue)
{
    if (m_fMinValue != minValue)
    {
        m_fMinValue = minValue;
        m_fValue = MAX(m_fValue, m_fMinValue);
        this->layoutSubViews();
    }
}

void CASlider::setMaxValue(float maxValue)
{
    if (m_fMaxValue != maxValue)
    {
        m_fMaxValue = maxValue;
        m_fValue = MIN(m_fValue, m_fMaxValue);
        this->layoutSubViews();
    }
}

void CASlider::setTrackHeight(float trackHeight)
{
    if (m_fTrackHeight != trackHeight)
    {
        m_fTrackHeight = trackHeight;
        this->layoutSubViews();
    }
}

void CASlider::setMinTrackTintImage(CAImage* image)
{
    if (m_pMinTrackTintImage != image)
    {
        CC_SAFE_RETAIN(image);
        CC_SAFE_RELEASE(m_pMinTrackTintImage);
        m_pMinTrackTintImage = image;
        if (m_pMinTrackTintImageView)
        {
            ((CAScale9ImageView*)m_pMinTrackTintImageView)->setImage(m_pMinTrackTintImage);
        }
    }
}

void CASlider::setMaxTrackTintImage(CAImage* image)
{
    if (m_pMaxTrackTintImage != image)
    {
        CC_SAFE_RETAIN(image);
        CC_SAFE_RELEASE(m_pMaxTrackTintImage);
        m_pMaxTrackTintImage = image;
        if (m_pMaxTrackTintImageView)
        {
            ((CAScale9ImageView*)m_pMaxTrackTintImageView)->setImage(m_pMaxTrackTintImage);
        }
    }
}

void CASlider::setThumbTintImage(CAImage* image)
{
    if (m_pThumbTintImage != image)
    {
        CC_SAFE_RETAIN(image);
        CC_SAFE_RELEASE(m_pThumbTintImage);
        m_pThumbTintImage = image;
        if (m_pThumbTintImageView)
        {
            m_pThumbTintImageView->setImage(m_pThumbTintImage);
            const DSize size = this->getBounds().size;
            m_pThumbTintImageView->setFrame(DRect(0, 0, size.height, size.height));
        }
        this->layoutSubViews();
    }
}

bool CASlider::ccTouchBegan(CATouch *pTouch, CAEvent *pEvent)
{
    DPoint point = pTouch->getLocation();
    point = this->convertToNodeSpace(point);
    
	m_bTouchClick = true;
    return m_bTouchClick;
}

void CASlider::ccTouchMoved(CrossApp::CATouch *pTouch, CrossApp::CAEvent *pEvent)
{
    DPoint point = pTouch->getLocation();
    point = this->convertToNodeSpace(point);
    
    if (!this->isTouchClick())
        return;
    
    DRect bounds = getBounds();
    float value = (m_fMaxValue - m_fMinValue) * (point.x / bounds.size.width) + m_fMinValue;
    this->setValue(value);
    m_function(this, value);
}

void CASlider::ccTouchEnded(CrossApp::CATouch *pTouch, CrossApp::CAEvent *pEvent)
{
    if (!this->isTouchClick())
        return;
    
	m_bTouchClick = false;

    DPoint point = pTouch->getLocation();
    point = this->convertToNodeSpace(point);
    DRect bounds = getBounds();
    if (bounds.containsPoint(point))
    {
        float value = (m_fMaxValue - m_fMinValue) * (point.x / bounds.size.width) + m_fMinValue;
        this->setValue(value);
        m_function(this, value);
    }
    
    m_functionTouchUpSide(this, m_fValue);
}

void CASlider::setTarget(const std::function<void (CASlider*, float)>& function)
{
    m_function = function;
}

void CASlider::setTargetForTouchUpSide(const std::function<void (CASlider*, float)>& function)
{
    m_functionTouchUpSide = function;
}

void CASlider::setContentSize(const DSize & var)
{
    DSize size = var;
    if (m_bRecSpe)
    {
        const CAThemeManager::stringMap& map = CAApplication::getApplication()->getThemeManager()->getThemeMap("CASlider");
        int h = atoi(map.at("height").c_str());
        size.height = (h == 0) ? size.height : h;
    }
    CAControl::setContentSize(size);
    
    this->layoutSubViews();
}

NS_CC_END
