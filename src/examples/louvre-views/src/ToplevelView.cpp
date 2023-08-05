#include <LTextureView.h>
#include <LSceneView.h>
#include <LCursor.h>
#include <LXCursor.h>
#include <LTime.h>
#include <LLog.h>

#include "ToplevelButton.h"
#include "ToplevelView.h"
#include "Toplevel.h"
#include "Surface.h"
#include "Global.h"
#include "Pointer.h"
#include "InputRect.h"
#include "Output.h"
#include "Topbar.h"

static void onPointerEnterResizeArea(InputRect *rect, void *data, const LPoint &)
{
    ToplevelView *view = (ToplevelView*)rect->parent();
    Pointer *pointer = (Pointer*)view->seat()->pointer();
    LXCursor *cursor = (LXCursor*)data;

    if (pointer->resizingToplevel() || pointer->movingToplevel())
        return;

    if (data)
    {
        G::compositor()->cursor()->setTextureB(cursor->texture(), cursor->hotspotB());
        pointer->cursorOwner = view;
    }
    else
        G::compositor()->cursor()->useDefault();

    G::compositor()->cursor()->setVisible(true);
}

static void onPointerLeaveResizeArea(InputRect *rect, void *data)
{
    ToplevelView *view = (ToplevelView*)rect->parent();
    Pointer *pointer = (Pointer*)view->seat()->pointer();

    if (data && pointer->cursorOwner == view)
    {
        pointer->cursorOwner = nullptr;
        G::compositor()->updatePointerBeforePaint = true;
    }
}

static void onPointerButtonResizeArea(InputRect *rect, void *data, LPointer::Button button, LPointer::ButtonState state)
{
    ToplevelView *view = (ToplevelView*)rect->parent();
    Pointer *pointer = (Pointer*)view->seat()->pointer();
    Toplevel *toplevel = view->toplevel;

    if (button != LPointer::Left)
        return;

    if (state == LPointer::Pressed)
    {
        pointer->setFocus(toplevel->surface());
        view->seat()->keyboard()->setFocus(toplevel->surface());

        if (!toplevel->activated())
            toplevel->configure(toplevel->states() | LToplevelRole::Activated);
        G::compositor()->raiseSurface(toplevel->surface());

        if (data)
            toplevel->startResizeRequest((LToplevelRole::ResizeEdge)rect->id);
        else
            toplevel->startMoveRequest();
    }
    // Maximize / unmaximize on topbar double click
    else if (!data)
    {
        UInt32 now = LTime::ms();

        if (now  - view->lastTopbarClickMs < 220)
        {
            if (view->toplevel->maximized())
                view->toplevel->unsetMaximizedRequest();
            else
                view->toplevel->setMaximizedRequest();
        }

        view->lastTopbarClickMs = now;
    }

    if (pointer->cursorOwner == view)
        pointer->cursorOwner = nullptr;
}

ToplevelView::ToplevelView(Toplevel *toplevel) : LLayerView(G::compositor()->surfacesLayer)
{
    this->toplevel = toplevel;
    toplevel->decoratedView = this;

    clipTop = new LLayerView(this);
    clipTop->setPos(0, 0);
    class Surface *surf = (class Surface*)toplevel->surface();
    setParent(surf->view->parent());
    surf->view->setPrimary(true);
    surf->view->enableCustomPos(true);
    surf->view->enableParentClipping(true);
    surf->view->setParent(clipTop);
    surf->view->setCustomPos(LPoint(0, 0));

    clipBottom = new LLayerView(this);
    surfB = new LSurfaceView(toplevel->surface(), clipBottom);
    surfB->setPrimary(false);
    surfB->enableParentClipping(true);
    surfB->enableCustomPos(true);

    sceneBL = new LSceneView(LSize(TOPLEVEL_BORDER_RADIUS, TOPLEVEL_BORDER_RADIUS) * 2, 2, this);
    sceneBR = new LSceneView(LSize(TOPLEVEL_BORDER_RADIUS, TOPLEVEL_BORDER_RADIUS) * 2, 2, this);

    surfBL = new LSurfaceView(toplevel->surface(), sceneBL);
    surfBR = new LSurfaceView(toplevel->surface(), sceneBR);

    // Make them always translucent
    surfBL->enableCustomTranslucentRegion(true);
    surfBR->enableCustomTranslucentRegion(true);

    // Make them not primary so that the damage of the surface won't be cleared after rendered
    surfBL->setPrimary(false);
    surfBR->setPrimary(false);

    // Ignore the pos given by the surface
    surfBL->enableCustomPos(true);
    surfBR->enableCustomPos(true);

    // Clipped to the bottom border radius rect
    surfBL->enableParentClipping(true);
    surfBR->enableParentClipping(true);

    // Clipped to the bottom border radius rect
    surfBL->enableParentScaling(false);
    surfBR->enableParentScaling(false);

    // Masks to make the lower corners of the toplevel round
    maskBL = new LTextureView(G::toplevelTextures().maskBL, sceneBL);
    maskBR = new LTextureView(G::toplevelTextures().maskBR, sceneBR);
    maskBL->setBufferScale(2);
    maskBR->setBufferScale(2);
    maskBL->setPos(0, 0);
    maskBR->setPos(0, 0);
    maskBL->enableParentScaling(false);
    maskBR->enableParentScaling(false);

    // This blending func makes the alpha of the toplevel be replaced by the one of the mask
    maskBL->setBlendFunc(GL_ZERO, GL_SRC_ALPHA);
    maskBR->setBlendFunc(GL_ZERO, GL_SRC_ALPHA);

    // Toplevel decorations (shadows and topbar)
    decoTL = new LTextureView(G::toplevelTextures().activeTL, this);
    decoT = new LTextureView(G::toplevelTextures().activeT, this);
    decoTR = new LTextureView(G::toplevelTextures().activeTR, this);
    decoL = new LTextureView(G::toplevelTextures().activeL, this);
    decoR = new LTextureView(G::toplevelTextures().activeR, this);
    decoBL = new LTextureView(G::toplevelTextures().activeBL, this);
    decoB = new LTextureView(G::toplevelTextures().activeB, this);
    decoBR = new LTextureView(G::toplevelTextures().activeBR, this);

    decoTL->setBufferScale(2);
    decoT->setBufferScale(2);
    decoTR->setBufferScale(2);
    decoL->setBufferScale(2);
    decoR->setBufferScale(2);
    decoBL->setBufferScale(2);
    decoB->setBufferScale(2);
    decoBR->setBufferScale(2);

    decoT->enableDstSize(true);
    decoL->enableDstSize(true);
    decoR->enableDstSize(true);
    decoB->enableDstSize(true);

    topbarInput = new InputRect(this, nullptr);
    resizeT = new InputRect(this, G::cursors().top_side, LToplevelRole::ResizeEdge::Top);
    resizeB = new InputRect(this, G::cursors().bottom_side, LToplevelRole::ResizeEdge::Bottom);
    resizeL = new InputRect(this, G::cursors().left_side, LToplevelRole::ResizeEdge::Left);
    resizeR = new InputRect(this, G::cursors().right_side, LToplevelRole::ResizeEdge::Right);
    resizeTL = new InputRect(this, G::cursors().top_left_corner, LToplevelRole::ResizeEdge::TopLeft);
    resizeTR = new InputRect(this, G::cursors().top_right_corner, LToplevelRole::ResizeEdge::TopRight);
    resizeBL = new InputRect(this, G::cursors().bottom_left_corner, LToplevelRole::ResizeEdge::BottomLeft);
    resizeBR = new InputRect(this, G::cursors().bottom_right_corner, LToplevelRole::ResizeEdge::BottomRight);

    resizeTL->onPointerEnter = &onPointerEnterResizeArea;
    resizeTR->onPointerEnter = &onPointerEnterResizeArea;
    resizeBL->onPointerEnter = &onPointerEnterResizeArea;
    resizeBR->onPointerEnter = &onPointerEnterResizeArea;
    resizeT->onPointerEnter = &onPointerEnterResizeArea;
    resizeB->onPointerEnter = &onPointerEnterResizeArea;
    resizeL->onPointerEnter = &onPointerEnterResizeArea;
    resizeR->onPointerEnter = &onPointerEnterResizeArea;
    topbarInput->onPointerEnter = &onPointerEnterResizeArea;

    resizeTL->onPointerLeave = &onPointerLeaveResizeArea;
    resizeTR->onPointerLeave = &onPointerLeaveResizeArea;
    resizeBL->onPointerLeave = &onPointerLeaveResizeArea;
    resizeBR->onPointerLeave = &onPointerLeaveResizeArea;
    resizeT->onPointerLeave = &onPointerLeaveResizeArea;
    resizeB->onPointerLeave = &onPointerLeaveResizeArea;
    resizeL->onPointerLeave = &onPointerLeaveResizeArea;
    resizeR->onPointerLeave = &onPointerLeaveResizeArea;
    topbarInput->onPointerLeave = &onPointerLeaveResizeArea;

    resizeTL->onPointerButton = &onPointerButtonResizeArea;
    resizeTR->onPointerButton = &onPointerButtonResizeArea;
    resizeBL->onPointerButton = &onPointerButtonResizeArea;
    resizeBR->onPointerButton = &onPointerButtonResizeArea;
    resizeT->onPointerButton = &onPointerButtonResizeArea;
    resizeB->onPointerButton = &onPointerButtonResizeArea;
    resizeL->onPointerButton = &onPointerButtonResizeArea;
    resizeR->onPointerButton = &onPointerButtonResizeArea;
    topbarInput->onPointerButton = &onPointerButtonResizeArea;

    // Buttons
    buttonsContainer = new InputRect(this, this);
    buttonsContainer->setPos(TOPLEVEL_BUTTON_SPACING, TOPLEVEL_BUTTON_SPACING - TOPLEVEL_TOPBAR_HEIGHT);
    buttonsContainer->setSize(3 * TOPLEVEL_BUTTON_SIZE + 2 * TOPLEVEL_BUTTON_SPACING, TOPLEVEL_BUTTON_SIZE);

    closeButton = new ToplevelButton(buttonsContainer, this, ToplevelButton::Close);
    minimizeButton = new ToplevelButton(buttonsContainer, this, ToplevelButton::Minimize);
    maximizeButton = new ToplevelButton(buttonsContainer, this, ToplevelButton::Maximize);

    closeButton->enableBlockPointer(false);
    minimizeButton->enableBlockPointer(false);
    maximizeButton->enableBlockPointer(false);

    minimizeButton->setPos(TOPLEVEL_BUTTON_SIZE + TOPLEVEL_BUTTON_SPACING, 0);
    maximizeButton->setPos(2 * (TOPLEVEL_BUTTON_SIZE + TOPLEVEL_BUTTON_SPACING), 0);

    buttonsContainer->onPointerEnter = [](InputRect *, void *data, const LPoint &)
    {
        ToplevelView *view = (ToplevelView*)data;
        Pointer *pointer = (Pointer*)view->seat()->pointer();

        if (view->seat()->pointer()->resizingToplevel())
            return;

        view->closeButton->update();
        view->minimizeButton->update();
        view->maximizeButton->update();

        if (!pointer->cursorOwner)
            view->cursor()->useDefault();
    };

    buttonsContainer->onPointerLeave = [](InputRect *, void *data)
    {
        ToplevelView *view = (ToplevelView*)data;

        if (view->seat()->pointer()->resizingToplevel())
            return;

        view->closeButton->update();
        view->minimizeButton->update();
        view->maximizeButton->update();
    };

    updateGeometry();
}

ToplevelView::~ToplevelView()
{
    Pointer *pointer = (Pointer*)seat()->pointer();

    if (pointer->cursorOwner == this)
    {
        pointer->cursorOwner = nullptr;
        cursor()->useDefault();
    }

    delete clipTop;
    delete clipBottom;
    delete surfB;
    delete decoTL;
    delete decoT;
    delete decoTR;
    delete decoL;
    delete decoR;
    delete decoBL;
    delete decoB;
    delete decoBR;
    delete maskBL;
    delete maskBR;
    delete sceneBL;
    delete sceneBR;
    delete surfBL;
    delete surfBR;

    delete resizeTL;
    delete resizeTR;
    delete resizeBL;
    delete resizeBR;
    delete resizeT;
    delete resizeB;
    delete resizeL;
    delete resizeR;
    delete topbarInput;

    delete buttonsContainer;
    delete closeButton;
    delete minimizeButton;
    delete maximizeButton;
}

void ToplevelView::updateGeometry()
{
    class Surface *surf = (class Surface *)toplevel->surface();

    if (surf->view->parent() != clipTop)
    {
        surf->view->setParent(clipTop);
        surf->view->insertAfter(nullptr);
    }

    if (toplevel->windowGeometry().size().area() == 0)
        return;

    int TOPLEVEL_TOP_LEFT_OFFSET_X,
        TOPLEVEL_TOP_LEFT_OFFSET_Y,
        TOPLEVEL_BOTTOM_LEFT_OFFSET_X,
        TOPLEVEL_BOTTOM_LEFT_OFFSET_Y,
        TOPLEVEL_TOP_CLAMP_OFFSET_Y,
        TOPLEVEL_MIN_WIDTH_TOP,
        TOPLEVEL_MIN_WIDTH_BOTTOM,
        TOPLEVEL_MIN_HEIGHT;

    if (toplevel->activated())
    {
        TOPLEVEL_TOP_LEFT_OFFSET_X = TOPLEVEL_ACTIVE_TOP_LEFT_OFFSET_X;
        TOPLEVEL_TOP_LEFT_OFFSET_Y = TOPLEVEL_ACTIVE_TOP_LEFT_OFFSET_Y;
        TOPLEVEL_BOTTOM_LEFT_OFFSET_X = TOPLEVEL_ACTIVE_BOTTOM_LEFT_OFFSET_X;
        TOPLEVEL_BOTTOM_LEFT_OFFSET_Y = TOPLEVEL_ACTIVE_BOTTOM_LEFT_OFFSET_Y;
        TOPLEVEL_TOP_CLAMP_OFFSET_Y = TOPLEVEL_ACTIVE_TOP_CLAMP_OFFSET_Y;
        TOPLEVEL_MIN_WIDTH_TOP = TOPLEVEL_ACTIVE_MIN_WIDTH_TOP;
        TOPLEVEL_MIN_WIDTH_BOTTOM = TOPLEVEL_ACTIVE_MIN_WIDTH_BOTTOM;
        TOPLEVEL_MIN_HEIGHT = TOPLEVEL_ACTIVE_MIN_HEIGHT;

        if (lastActiveState != toplevel->activated())
        {
            decoTL->setTexture(G::toplevelTextures().activeTL);
            decoT->setTexture(G::toplevelTextures().activeT);
            decoTR->setTexture(G::toplevelTextures().activeTR);
            decoL->setTexture(G::toplevelTextures().activeL);
            decoR->setTexture(G::toplevelTextures().activeR);
            decoBL->setTexture(G::toplevelTextures().activeBL);
            decoB->setTexture(G::toplevelTextures().activeB);
            decoBR->setTexture(G::toplevelTextures().activeBR);

            // Trans region
            decoTL->setTranslucentRegion(&G::toplevelTextures().activeTransRegionTL);
            decoTR->setTranslucentRegion(&G::toplevelTextures().activeTransRegionTR);
        }
    }
    else
    {
        TOPLEVEL_TOP_LEFT_OFFSET_X = TOPLEVEL_INACTIVE_TOP_LEFT_OFFSET_X;
        TOPLEVEL_TOP_LEFT_OFFSET_Y = TOPLEVEL_INACTIVE_TOP_LEFT_OFFSET_Y;
        TOPLEVEL_BOTTOM_LEFT_OFFSET_X = TOPLEVEL_INACTIVE_BOTTOM_LEFT_OFFSET_X;
        TOPLEVEL_BOTTOM_LEFT_OFFSET_Y = TOPLEVEL_INACTIVE_BOTTOM_LEFT_OFFSET_Y;
        TOPLEVEL_TOP_CLAMP_OFFSET_Y = TOPLEVEL_INACTIVE_TOP_CLAMP_OFFSET_Y;
        TOPLEVEL_MIN_WIDTH_TOP = TOPLEVEL_INACTIVE_MIN_WIDTH_TOP;
        TOPLEVEL_MIN_WIDTH_BOTTOM = TOPLEVEL_INACTIVE_MIN_WIDTH_BOTTOM;
        TOPLEVEL_MIN_HEIGHT = TOPLEVEL_INACTIVE_MIN_HEIGHT;

        if (lastActiveState != toplevel->activated())
        {
            decoTL->setTexture(G::toplevelTextures().inactiveTL);
            decoT->setTexture(G::toplevelTextures().inactiveT);
            decoTR->setTexture(G::toplevelTextures().inactiveTR);
            decoL->setTexture(G::toplevelTextures().inactiveL);
            decoR->setTexture(G::toplevelTextures().inactiveR);
            decoBL->setTexture(G::toplevelTextures().inactiveBL);
            decoB->setTexture(G::toplevelTextures().inactiveB);
            decoBR->setTexture(G::toplevelTextures().inactiveBR);

            // Trans region
            decoTL->setTranslucentRegion(&G::toplevelTextures().inactiveTransRegionTL);
            decoTR->setTranslucentRegion(&G::toplevelTextures().inactiveTransRegionTR);
        }
    }

    closeButton->update();
    minimizeButton->update();
    maximizeButton->update();
    lastActiveState = toplevel->activated();


    if (toplevel->fullscreen())
    {
        if (!lastFullscreenState)
        {
            surf->view->setCustomPos(0, 0);
            clipBottom->setVisible(false);
            sceneBL->setVisible(false);
            sceneBR->setVisible(false);
            maskBL->setVisible(false);
            maskBR->setVisible(false);
            decoTL->setVisible(false);
            decoTR->setVisible(false);
            decoL->setVisible(false);
            decoR->setVisible(false);
            decoBL->setVisible(false);
            decoBR->setVisible(false);
            decoB->setVisible(false);
            resizeT->setVisible(false);
            resizeB->setVisible(false);
            resizeL->setVisible(false);
            resizeR->setVisible(false);
            resizeTL->setVisible(false);
            resizeTR->setVisible(false);
            resizeBL->setVisible(false);
            resizeBR->setVisible(false);
        }

        setSize(toplevel->windowGeometry().size());

        LSize size = nativeSize();

        clipTop->setSize(size);

        decoT->setDstSize(size.w(), decoT->texture()->sizeB().h() / 2);
        decoT->setPos(0, -decoT->nativeSize().h() + (TOPLEVEL_TOPBAR_HEIGHT + TOPLEVEL_TOP_CLAMP_OFFSET_Y) * toplevel->fullscreenOutput->topbar->visiblePercent);
        buttonsContainer->setPos(TOPLEVEL_BUTTON_SPACING, TOPLEVEL_BUTTON_SPACING - TOPLEVEL_TOPBAR_HEIGHT * (1.f - toplevel->fullscreenOutput->topbar->visiblePercent));

        // Set topbar center translucent regions
        LRegion transT;
        transT.addRect(
            0,
            0,
            decoT->size().w(),
            decoT->size().h() - TOPLEVEL_TOPBAR_HEIGHT - TOPLEVEL_TOP_CLAMP_OFFSET_Y);
        transT.addRect(
            0,
            decoT->size().h() - TOPLEVEL_TOP_CLAMP_OFFSET_Y,
            decoT->size().w(),
            TOPLEVEL_TOP_CLAMP_OFFSET_Y);
        decoT->setTranslucentRegion(&transT);

        topbarInput->setPos(0, - TOPLEVEL_TOPBAR_HEIGHT);
        topbarInput->setSize(size.w(), TOPLEVEL_TOPBAR_HEIGHT);
    }
    else
    {
        if (lastFullscreenState)
        {
            buttonsContainer->setPos(TOPLEVEL_BUTTON_SPACING, TOPLEVEL_BUTTON_SPACING - TOPLEVEL_TOPBAR_HEIGHT);
            surf->view->setCustomPos(0, 0);
            clipBottom->setVisible(true);
            sceneBL->setVisible(true);
            sceneBR->setVisible(true);
            maskBL->setVisible(true);
            maskBR->setVisible(true);
            decoTL->setVisible(true);
            decoTR->setVisible(true);
            decoL->setVisible(true);
            decoR->setVisible(true);
            decoBL->setVisible(true);
            decoBR->setVisible(true);
            decoB->setVisible(true);
            resizeT->setVisible(true);
            resizeB->setVisible(true);
            resizeL->setVisible(true);
            resizeR->setVisible(true);
            resizeTL->setVisible(true);
            resizeTR->setVisible(true);
            resizeBL->setVisible(true);
            resizeBR->setVisible(true);
        }

        Int32 clip = 1;

        setSize(
            toplevel->windowGeometry().size().w() - 2 * clip,
            toplevel->windowGeometry().size().h() - 2 * clip);

        LSize size = nativeSize();

        // Upper surface view
        clipTop->setSize(
            size.w(),
            size.h() - TOPLEVEL_BORDER_RADIUS);
        surf->view->setCustomPos(- clip, - clip);

        // Lower surface view (without border radius rects)
        clipBottom->setPos(
            TOPLEVEL_BORDER_RADIUS,
            size.h() - TOPLEVEL_BORDER_RADIUS);
        clipBottom->setSize(
            size.w() - 2 * TOPLEVEL_BORDER_RADIUS,
            TOPLEVEL_BORDER_RADIUS);
        surfB->setCustomPos(
            - TOPLEVEL_BORDER_RADIUS - clip,
            TOPLEVEL_BORDER_RADIUS - size.h() - clip);

        // Bottom left / right surfaces views
        sceneBL->setPos(
            0,
            size.h() - TOPLEVEL_BORDER_RADIUS);
        sceneBR->setPos(
            size.w() - TOPLEVEL_BORDER_RADIUS,
            size.h() - TOPLEVEL_BORDER_RADIUS);
        surfBL->setCustomPos(
            - clip,
            TOPLEVEL_BORDER_RADIUS - size.h() - clip);
        surfBR->setCustomPos(
            TOPLEVEL_BORDER_RADIUS - size.w() - clip,
            TOPLEVEL_BORDER_RADIUS - size.h() - clip);

        // Decorations
        decoTL->setPos(
            TOPLEVEL_TOP_LEFT_OFFSET_X,
            TOPLEVEL_TOP_LEFT_OFFSET_Y);
        decoT->setDstSize(
            size.w() - TOPLEVEL_MIN_WIDTH_TOP,
            decoT->texture()->sizeB().h() / 2);
        decoT->setPos(
            decoTL->nativePos().x() + decoTL->nativeSize().w(),
            -decoT->nativeSize().h() + TOPLEVEL_TOP_CLAMP_OFFSET_Y);
        decoTR->setPos(
            size.w() - decoTR->nativeSize().w() - TOPLEVEL_TOP_LEFT_OFFSET_X,
            TOPLEVEL_TOP_LEFT_OFFSET_Y);
        decoL->setDstSize(
            decoL->texture()->sizeB().w() / 2,
            size.h() - TOPLEVEL_MIN_HEIGHT);
        decoL->setPos(
            -decoL->nativeSize().w(),
            decoTL->nativePos().y() + decoTL->nativeSize().h());
        decoR->setDstSize(
            decoR->texture()->sizeB().w() / 2,
            size.h() - TOPLEVEL_MIN_HEIGHT);
        decoR->setPos(
            size.w(),
            decoTL->nativePos().y() + decoTL->nativeSize().h());
        decoBL->setPos(
            TOPLEVEL_BOTTOM_LEFT_OFFSET_X,
            size.h() + TOPLEVEL_BOTTOM_LEFT_OFFSET_Y);
        decoBR->setPos(
            size.w() - decoBR->nativeSize().w() - TOPLEVEL_BOTTOM_LEFT_OFFSET_X,
            size.h() + TOPLEVEL_BOTTOM_LEFT_OFFSET_Y);
        decoB->setDstSize(
            size.w() - TOPLEVEL_MIN_WIDTH_BOTTOM,
            decoB->texture()->sizeB().h() / 2);
        decoB->setPos(
            decoBL->nativePos().x() + decoBL->nativeSize().w(),
            size.h());

        // Set topbar center translucent regions
        LRegion transT;
        transT.addRect(
            0,
            0,
            decoT->size().w(),
            decoT->size().h() - TOPLEVEL_TOPBAR_HEIGHT - TOPLEVEL_TOP_CLAMP_OFFSET_Y);
        transT.addRect(
            0,
            decoT->size().h() - TOPLEVEL_TOP_CLAMP_OFFSET_Y,
            decoT->size().w(),
            TOPLEVEL_TOP_CLAMP_OFFSET_Y);
        decoT->setTranslucentRegion(&transT);

        // Update input rects
        resizeT->setPos(0, - TOPLEVEL_TOPBAR_HEIGHT - TOPLEVEL_RESIZE_INPUT_MARGIN);
        resizeT->setSize(size.w(), TOPLEVEL_RESIZE_INPUT_MARGIN * 2);

        resizeB->setPos(0, size.h() - TOPLEVEL_RESIZE_INPUT_MARGIN);
        resizeB->setSize(size.w(), TOPLEVEL_RESIZE_INPUT_MARGIN * 2);

        resizeL->setPos(-TOPLEVEL_RESIZE_INPUT_MARGIN, - TOPLEVEL_TOPBAR_HEIGHT);
        resizeL->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN * 2, size.h() + TOPLEVEL_TOPBAR_HEIGHT);

        resizeR->setPos(size.w() - TOPLEVEL_RESIZE_INPUT_MARGIN, - TOPLEVEL_TOPBAR_HEIGHT);
        resizeR->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN * 2, size.h() + TOPLEVEL_TOPBAR_HEIGHT);

        resizeTL->setPos(-TOPLEVEL_RESIZE_INPUT_MARGIN, - TOPLEVEL_TOPBAR_HEIGHT - TOPLEVEL_RESIZE_INPUT_MARGIN);
        resizeTL->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS, TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS);

        resizeTR->setPos(size.w() - TOPLEVEL_BORDER_RADIUS, - TOPLEVEL_TOPBAR_HEIGHT - TOPLEVEL_RESIZE_INPUT_MARGIN);
        resizeTR->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS, TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS);

        resizeBL->setPos(-TOPLEVEL_RESIZE_INPUT_MARGIN, size.h() - TOPLEVEL_BORDER_RADIUS);
        resizeBL->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS, TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS);

        resizeBR->setPos(size.w() - TOPLEVEL_BORDER_RADIUS, size.h() - TOPLEVEL_BORDER_RADIUS);
        resizeBR->setSize(TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS, TOPLEVEL_RESIZE_INPUT_MARGIN + TOPLEVEL_BORDER_RADIUS);

        topbarInput->setPos(0, - TOPLEVEL_TOPBAR_HEIGHT);
        topbarInput->setSize(size.w(), TOPLEVEL_TOPBAR_HEIGHT);
    }

    lastFullscreenState = toplevel->fullscreen();
}

bool ToplevelView::nativeMapped() const
{
    return toplevel->surface()->mapped();
}

const LPoint &ToplevelView::nativePos() const
{
    return toplevel->rolePos();
}

void ToplevelView::keyEvent(UInt32 keyCode, UInt32 keyState)
{
    L_UNUSED(keyState);

    if (keyCode == KEY_LEFTALT)
        maximizeButton->update();
}
