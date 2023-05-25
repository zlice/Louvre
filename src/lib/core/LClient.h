#ifndef LCLIENT_H
#define LCLIENT_H

#include <list>
#include <LRegion.h>
#include <LSurface.h>

using namespace std;

/*!
 * @brief Representation of a Wayland client.
 *
 * The LClient class represents a Wayland client connected to the compositor. Provides access to many client resources generated by the original Wayland library, 
 * to its surfaces, last serials used by the protocols, among other properties.
 */
class Louvre::LClient
{
public:

    struct Params;

    /*!
     * @brief Constructor of the LClient class.
     *
     * @param params Internal library parameters passed int the LCompositor::createClientRequest virtual constructor.
     */
    LClient(Params *params);

    /*!
     * @brief Destructor de the LClient class.
     */
    ~LClient();

    LClient(const LClient&) = delete;
    LClient& operator= (const LClient&) = delete;

    /*!
     * @brief Returns a pointer to the compositor instance.
     *
     * This instance is the same for all clients. Not to be confused with the **wl_composer** interface resource
     * of the Wayland protocol returned by LClient::composerResource() which is unique per client.
     */
    LCompositor *compositor() const;

    /*!
     * @brief Retorna un puntero al asiento de eventos del compositor.
     *
     * Retorna la instancia global LSeat creada durante la inicialización del compositor. No confundir con el recurso
     * de la interfaz **wl_seat** del protocolo de Wayland retornado por LClient::seatResource() que sí es único por cliente.
     */
    LSeat *seat() const;

    /*!
     * @brief Returns the **wl_client** interface of the client.
     *
     * The **wl_client** interface is part of the original Wayland library.
     */
    wl_client *client() const;

    /*!
     * @brief Client's data device.
     *
     * The LDataDevice class is a wrapper for the **wl_data_device** interface of the Wayland protocol
     * , used by the clipboard mechanism and for drag & drop sessions.\n
     */
    LDataDevice &dataDevice() const;

    /*!
     * @brief List of surfaces created by the client.
     */
    const list<LSurface*>&surfaces() const;

    /*!
     * @brief List of **wl_output** resources.
     *
     * Returns a list of **wl_output** resources of the Wayland protocol.\n
     * The library creates a **wl_output** resource for each output added to the composer in order to notify the client
     * the available outputs and their attributes.
     */
    const list<Protocols::Wayland::GOutput*>&outputGlobals() const;

    /*!
     * @brief Resource generated when the client binds to the **wl_compositor** singleton global of the Wayland protocol.
     */
    Protocols::Wayland::GCompositor *compositorGlobal() const;

    /*!
     * @brief List of resources generated when the client binds to the **wl_subcompositor** global of the Wayland protocol.
     */
    list<Protocols::Wayland::GSubcompositor*> &subcompositorGlobals() const;

    /*!
     * @brief List of resources generated when the client binds to the **wl_seat** global of the Wayland protocol.
     */
    list<Protocols::Wayland::GSeat*> &seatGlobals() const;

    /*!
     * @brief Resource generated when the client binds to the **wl_data_device_manager** singleton global of the Wayland protocol.
     */
    Protocols::Wayland::GDataDeviceManager* dataDeviceManagerGlobal() const;

    /*!
     * @brief Returns the resource generated by the **xdg_wm_base** interface of the XDG Shell protocol.
     *
     * The **xdg_wm_base** interface is unique per client and its purpose is to allow the client to create Toplevel and Popup roles for surfaces.\n
     *
     * Returns ***nullptr*** if the client has not created the resource.
     */
    wl_resource *xdgWmBaseResource() const;

    /*!
     * @brief Returns the resource generated by the **zxdg_decoration_manager_v1** interface of the XDG Decoration protocol.
     *
     * The **zxdg_decoration_manager_v1** interface is unique per client and its purpose is to allow the client to negotiate
     * with the compositor who should be in charge of rendering the decoration of a Toplevel surface.\n
     *
     * Returns ***nullptr*** if the client has not created the resource.
     */
    wl_resource *xdgDecorationManagerResource() const;

    /*!
     * @brief Returns the resource generated by the **wl_touch** interface of the Wayland protocol.
     *
     * The **wl_touch** interface is unique per client and its purpose is to allow the client to receive touch events.\n
     *
     * Returns ***nullptr*** if the client has not created the resource.
     */
    wl_resource *touchResource() const;

    LPRIVATE_IMP(LClient)

};

#endif // LCLIENT_H
