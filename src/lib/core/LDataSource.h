#ifndef LDATASOURCE_H
#define LDATASOURCE_H

#include <LNamespaces.h>
#include <bits/types/FILE.h>

/*!
 * @brief Data source exchanged between clients
 *
 * The LDataSource class represents the **wl_data_source** interface of the Wayland protocol. It is created by clients to define information
 * that can be shared with other clients through the clipboard or in drag & drop sessions.\n
 *
 * See the LDataDevice and LDataOffer classes documentation for more information.
 */
class Louvre::LDataSource
{
public:   

    /*!
     * @brief Variant of a data source.
     *
     * Provides access to the information of data source for a specific mime type.\n
     */
    struct LSource
    {
        /// @brief File descriptor for a specific mime type
        FILE *tmp;

        /// @brief Mime type
        char *mimeType;
    };

    LDataSource(const LDataSource&) = delete;
    LDataSource& operator= (const LDataSource&) = delete;

    /*!
     * @brief Client owner of the data source.
     */
    LClient *client() const;

    /*!
     * @brief List of data source file descriptors.
     *
     * List of data source file descriptors for specific mime types.
     */
    const std::list<LSource>&sources() const;

#if LOUVRE_DATA_DEVICE_MANAGER_VERSION >= 3

    /*!
     * @brief Actions available for drag & drop sessions.
     *
     * Bitfield indicating the actions available for a drag & drop session.\n
     * Bitfield flags are described in LDNDManager::Action.
     */
    UInt32 dndActions() const;
#endif

    /*!
     * @brief Wayland resource of the data source.
     *
     * Returns the resource generated by the **wl_data_source** interface of the Wayland protocol.
     */
    Protocols::Wayland::DataSourceResource *dataSourceResource() const;

    class LDataSourcePrivate;

    /*!
     * @brief Access to the private API of LDataSource.
     *
     * Returns an instance of the LDataSourcePrivate class (following the ***PImpl Idiom*** pattern) which contains all the private members of LDataSource.\n
     * Used internally by the library.
     */
    LDataSourcePrivate *imp() const;
private:
    friend class Protocols::Wayland::DataSourceResource;
    friend class Protocols::Wayland::DataDeviceResource;
    LDataSource(Protocols::Wayland::DataSourceResource *dataSourceResource);
    ~LDataSource();
    LDataSourcePrivate *m_imp = nullptr;
};

#endif // LDATASOURCE_H
