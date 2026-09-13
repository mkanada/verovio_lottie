/////////////////////////////////////////////////////////////////////////////
// Name:        lottiedevicecontext.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "lottiedevicecontext.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cassert>

//----------------------------------------------------------------------------

#include "atts_shared.h"
#include "object.h"

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// LottieDeviceContext
//----------------------------------------------------------------------------

LottieDeviceContext::LottieDeviceContext() : DeviceContext(LOTTIE_DEVICE_CONTEXT) {}

LottieDeviceContext::~LottieDeviceContext() {}

void LottieDeviceContext::SetBackground(int color, int style) {}

void LottieDeviceContext::SetBackgroundImage(void *image, double opacity) {}

void LottieDeviceContext::SetBackgroundMode(int mode) {}

void LottieDeviceContext::SetTextForeground(int color) {}

void LottieDeviceContext::SetTextBackground(int color) {}

void LottieDeviceContext::SetLogicalOrigin(int x, int y)
{
    m_originX = -x;
    m_originY = -y;
}

Point LottieDeviceContext::GetLogicalOrigin()
{
    return Point(m_originX, m_originY);
}

void LottieDeviceContext::DrawQuadBezierPath(Point bezier[3]) {}

void LottieDeviceContext::DrawCubicBezierPath(Point bezier[4]) {}

void LottieDeviceContext::DrawCubicBezierPathFilled(Point bezier1[4], Point bezier2[4]) {}

void LottieDeviceContext::DrawBentParallelogramFilled(Point side[4], int height) {}

void LottieDeviceContext::DrawCircle(int x, int y, int radius) {}

void LottieDeviceContext::DrawEllipse(int x, int y, int width, int height) {}

void LottieDeviceContext::DrawEllipticArc(int x, int y, int width, int height, double start, double end) {}

void LottieDeviceContext::DrawLine(int x1, int y1, int x2, int y2) {}

void LottieDeviceContext::DrawPolyline(int n, Point points[], bool close) {}

void LottieDeviceContext::DrawPolygon(int n, Point points[]) {}

void LottieDeviceContext::DrawRectangle(int x, int y, int width, int height) {}

void LottieDeviceContext::DrawRotatedText(const std::string &text, int x, int y, double angle) {}

void LottieDeviceContext::DrawRoundedRectangle(int x, int y, int width, int height, int radius) {}

void LottieDeviceContext::DrawText(
    const std::string &text, const std::u32string &wtext, int x, int y, int width, int height)
{
}

void LottieDeviceContext::DrawMusicText(const std::u32string &text, int x, int y, bool setSmuflGlyph) {}

void LottieDeviceContext::DrawSpline(int n, Point points[]) {}

void LottieDeviceContext::DrawGraphicUri(int x, int y, int width, int height, const std::string &uri) {}

void LottieDeviceContext::DrawSvgShape(int x, int y, int width, int height, double scale, pugi::xml_node svg) {}

void LottieDeviceContext::DrawBackgroundImage(int x, int y) {}

void LottieDeviceContext::StartText(int x, int y, data_HORIZONTALALIGNMENT alignment) {}

void LottieDeviceContext::EndText() {}

void LottieDeviceContext::MoveTextTo(int x, int y, data_HORIZONTALALIGNMENT alignment) {}

void LottieDeviceContext::MoveTextVerticallyTo(int y) {}

void LottieDeviceContext::StartGraphic(
    Object *object, const std::string &gClass, const std::string &gId, GraphicID graphicID, bool prepend)
{
    assert(!m_nodeStack.empty());

    std::unique_ptr<LottieNode> node = std::make_unique<LottieNode>();
    node->id = (graphicID == PRIMARY) ? gId : "";
    node->className = object->GetClassName();
    if (!gClass.empty()) {
        node->className.append(" " + gClass);
    }

    if (object->HasAttClass(ATT_COLOR)) {
        AttColor *att = dynamic_cast<AttColor *>(object);
        assert(att);
        if (att->HasColor()) {
            node->colorCss = att->GetColor();
        }
    }

    if (object->HasAttClass(ATT_VISIBILITY)) {
        AttVisibility *att = dynamic_cast<AttVisibility *>(object);
        assert(att);
        if (att->HasVisible() && (att->GetVisible() == BOOLEAN_false)) {
            node->hidden = true;
        }
    }

    LottieNode *current = m_nodeStack.back();
    LottieNode *added = node.get();

    LottieChild child;
    child.group = std::move(node);

    if (prepend) {
        current->children.insert(current->children.begin(), std::move(child));
    }
    else {
        current->children.push_back(std::move(child));
    }

    m_nodeStack.push_back(added);

    if (!added->id.empty()) {
        m_idMap[added->id] = added;
    }
}

void LottieDeviceContext::EndGraphic(Object *object, View *view)
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::StartCustomGraphic(const std::string &name, std::string gClass, std::string gId)
{
    assert(!m_nodeStack.empty());

    std::unique_ptr<LottieNode> node = std::make_unique<LottieNode>();
    node->id = gId;
    node->className = name;
    if (!gClass.empty()) {
        node->className.append(" " + gClass);
    }

    LottieNode *current = m_nodeStack.back();
    LottieNode *added = node.get();

    LottieChild child;
    child.group = std::move(node);
    current->children.push_back(std::move(child));

    m_nodeStack.push_back(added);

    if (!added->id.empty()) {
        m_idMap[added->id] = added;
    }
}

void LottieDeviceContext::EndCustomGraphic()
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::SetCustomGraphicColor(const std::string &color)
{
    assert(!m_nodeStack.empty());
    m_nodeStack.back()->colorCss = color;
}

void LottieDeviceContext::ResumeGraphic(Object *object, std::string gId)
{
    assert(!m_nodeStack.empty());

    std::map<std::string, LottieNode *>::iterator it = m_idMap.find(gId);
    if (it != m_idMap.end()) {
        m_nodeStack.push_back(it->second);
    }
    else {
        m_nodeStack.push_back(m_nodeStack.back());
    }
}

void LottieDeviceContext::EndResumedGraphic(Object *object, View *view)
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::RotateGraphic(Point const &orig, double angle)
{
    assert(!m_nodeStack.empty());

    LottieNode *node = m_nodeStack.back();
    if (node->hasRotation) {
        return;
    }
    node->hasRotation = true;
    node->rotation = angle;
    node->rotationOrigin = orig;
}

void LottieDeviceContext::StartPage()
{
    LottiePage page;
    page.root = std::make_unique<LottieNode>();
    page.width = this->GetWidth();
    page.height = this->GetHeight();
    page.contentHeight = this->GetContentHeight();
    std::pair<int, int> baseSize = this->GetBaseSize();
    page.baseWidth = baseSize.first;
    page.baseHeight = baseSize.second;
    page.userScaleX = this->GetUserScaleX();
    page.userScaleY = this->GetUserScaleY();
    page.viewBoxFactor = this->GetViewBoxFactor();
    page.originX = m_originX;
    page.originY = m_originY;

    m_pages.push_back(std::move(page));
    m_idMap.clear();
    m_nodeStack.clear();
    m_nodeStack.push_back(m_pages.back().root.get());
}

void LottieDeviceContext::EndPage()
{
    assert(m_nodeStack.size() == 1);
    m_nodeStack.clear();
}

void LottieDeviceContext::AddShape(LottieShape &&shape)
{
    assert(!m_nodeStack.empty());

    LottieNode *node = m_nodeStack.back();

    std::vector<LottieChild>::iterator firstGroup = std::find_if(node->children.begin(), node->children.end(),
        [](const LottieChild &child) { return (child.group != NULL); });

    LottieChild child;
    child.shape = std::move(shape);

    if (firstGroup != node->children.end()) {
        node->children.insert(firstGroup, std::move(child));
    }
    else if (m_pushBack) {
        node->children.insert(node->children.begin(), std::move(child));
    }
    else {
        node->children.push_back(std::move(child));
    }
}

} // namespace vrv
