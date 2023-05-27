#ifndef LCLIENTPRIVATE_H
#define LCLIENTPRIVATE_H

#include <LClient.h>
#include <LDataDevice.h>

using namespace Louvre::Protocols;

struct LClient::Params
{
    wl_client *client;
    LCompositor *compositor;
};

LPRIVATE_CLASS(LClient)
    std::list<LSurface*> surfaces;

    LClient::Params *params = nullptr;
    LDataDevice dataDevice;

    list<Wayland::GOutput*> outputGlobals;
    list<Wayland::GSeat*> seatGlobals;
    list<Wayland::GSubcompositor*> subcompositorGlobals;
    list<XdgShell::GXdgWmBase*> xdgWmBaseGlobals;
    list<XdgDecoration::GXdgDecorationManager*> xdgDecorationManagerGlobals;
    list<WpPresentationTime::GWpPresentation*> wpPresentationTimeGlobals;

    Wayland::GDataDeviceManager *dataDeviceManagerGlobal = nullptr;
    Wayland::GCompositor *compositorGlobal = nullptr;

    wl_resource         *linuxDMABufResource            = nullptr;
};



#endif // LCLIENTPRIVATE_H
