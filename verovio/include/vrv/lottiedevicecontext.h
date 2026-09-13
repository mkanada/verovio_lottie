/////////////////////////////////////////////////////////////////////////////
// Name:        lottiedevicecontext.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_DC_H__
#define __VRV_LOTTIE_DC_H__

#include <map>
#include <vector>

//----------------------------------------------------------------------------

#include "devicecontext.h"
#include "lottiegeometry.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottieDeviceContext
//----------------------------------------------------------------------------

/**
 * This class implements a drawing context for generating dotLottie files.
 * It is fed by the same View used for drawing SVG in order to guarantee
 * visual parity with the SVG output.
 */
class LottieDeviceContext : public DeviceContext {
public:
    /**
     * @name Constructors, destructors, and other standard methods
     */
    ///@{
    LottieDeviceContext();
    virtual ~LottieDeviceContext();
    ///@}

    /**
     * @name Setters
     */
    ///@{
    void SetBackground(int color, int style = PEN_SOLID) override;
    void SetBackgroundImage(void *image, double opacity = 1.0) override;
    void SetBackgroundMode(int mode) override;
    void SetTextForeground(int color) override;
    void SetTextBackground(int color) override;
    void SetLogicalOrigin(int x, int y) override;
    ///@}

    /**
     * @name Getters
     */
    ///@{
    Point GetLogicalOrigin() override;
    ///@}

    /**
     * @name Drawing methods
     */
    ///@{
    void DrawQuadBezierPath(Point bezier[3]) override;
    void DrawCubicBezierPath(Point bezier[4]) override;
    void DrawCubicBezierPathFilled(Point bezier1[4], Point bezier2[4]) override;
    void DrawBentParallelogramFilled(Point side[4], int height) override;
    void DrawCircle(int x, int y, int radius) override;
    void DrawEllipse(int x, int y, int width, int height) override;
    void DrawEllipticArc(int x, int y, int width, int height, double start, double end) override;
    void DrawLine(int x1, int y1, int x2, int y2) override;
    void DrawPolyline(int n, Point points[], bool close = false) override;
    void DrawPolygon(int n, Point points[]) override;
    void DrawRectangle(int x, int y, int width, int height) override;
    void DrawRotatedText(const std::string &text, int x, int y, double angle) override;
    void DrawRoundedRectangle(int x, int y, int width, int height, int radius) override;
    void DrawText(const std::string &text, const std::u32string &wtext = U"", int x = VRV_UNSET, int y = VRV_UNSET,
        int width = VRV_UNSET, int height = VRV_UNSET) override;
    void DrawMusicText(const std::u32string &text, int x, int y, bool setSmuflGlyph = false) override;
    void DrawSpline(int n, Point points[]) override;
    void DrawGraphicUri(int x, int y, int width, int height, const std::string &uri) override;
    void DrawSvgShape(int x, int y, int width, int height, double scale, pugi::xml_node svg) override;
    void DrawBackgroundImage(int x = 0, int y = 0) override;
    ///@}

    /**
     * @name Method for starting and ending a text
     */
    ///@{
    void StartText(int x, int y, data_HORIZONTALALIGNMENT alignment = HORIZONTALALIGNMENT_left) override;
    void EndText() override;

    /**
     * @name Move a text to the specified position, for example when starting a new line.
     */
    ///@{
    void MoveTextTo(int x, int y, data_HORIZONTALALIGNMENT alignment) override;
    void MoveTextVerticallyTo(int y) override;
    ///@}

    /**
     * Indicate if offset should be applied
     */
    bool ApplyOffset() override { return true; }

    /**
     * @name Method for starting and ending a graphic
     */
    ///@{
    void StartGraphic(Object *object, const std::string &gClass, const std::string &gId, GraphicID graphicID = PRIMARY,
        bool prepend = false) override;
    void EndGraphic(Object *object, View *view) override;
    ///@}

    /**
     * @name Method for starting and ending a graphic custom graphic that do not correspond to an Object
     */
    ///@{
    void StartCustomGraphic(const std::string &name, std::string gClass = "", std::string gId = "") override;
    void EndCustomGraphic() override;
    ///@}

    /**
     * Method for changing the color of a custom graphic
     */
    void SetCustomGraphicColor(const std::string &color) override;

    /**
     * @name Methods for re-starting and ending a graphic for objects drawn in separate steps
     */
    ///@{
    void ResumeGraphic(Object *object, std::string gId) override;
    void EndResumedGraphic(Object *object, View *view) override;
    ///@}

    /**
     * @name Method for rotating a graphic (clockwise).
     */
    ///@{
    void RotateGraphic(Point const &orig, double angle) override;
    ///@}

    /**
     * @name Method for starting and ending page
     */
    ///@{
    void StartPage() override;
    void EndPage() override;
    ///@}

    /**
     * Accessor to the pages built during drawing
     */
    const std::vector<LottiePage> &GetPages() const { return m_pages; }

public:
    //
private:
    /**
     * Insert a shape into the current node, replicating SvgDeviceContext::AddChild:
     * before the first child that is a subgroup, otherwise at the front (m_pushBack)
     * or at the back.
     */
    void AddShape(LottieShape &&shape);

private:
    int m_originX = 0;
    int m_originY = 0;

    std::vector<LottiePage> m_pages;
    std::vector<LottieNode *> m_nodeStack;
    std::map<std::string, LottieNode *> m_idMap;
};

} // namespace vrv

#endif // __VRV_LOTTIE_DC_H__
