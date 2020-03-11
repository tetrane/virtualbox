#ifndef TETRANE_VBOX_PLUGIN_H
#define TETRANE_VBOX_PLUGIN_H

#include <vector>
#include <VBox/vmm/tetrane.h>

#define OPEN_PLUGIN_CALLBACK_NAME "open_plugin"
#define CLOSE_PLUGIN_CALLBACK_NAME "close_plugin"
#define SYNC_POINT_CALLBACK_NAME "on_sync_point"
#define HW_ACCESS_CALLBACK_NAME "on_hardware_access"
#define COMMAND_CALLBACK_NAME "on_command"

namespace tetrane {
namespace tweaks {

class vm_tweaks;

extern "C" {

//! Context exported to plugins
struct plugin_binding
{
    int (*write_core)(plugin_binding*, const char*);
    uint64_t (*read_tsc)(plugin_binding*);

    tetrane::tweaks::vm_tweaks* impl;
};

typedef void (*open_plugin_t)(plugin_binding*);
typedef void (*close_plugin_t)(plugin_binding*);
typedef void (*sync_point_callback_t)(plugin_binding*, const sync_point* sp);
typedef void (*hardware_access_callback_t)(plugin_binding*, const saved_hardware_access* access, const unsigned char* data);
typedef void (*command_callback_t)(plugin_binding*);

} // extern "C"

//! Plugin handle class
//! note: plugins are unique, i.e. Multiple attempt to load the same plugin will result in equal (==) plugin objects.
class plugin
{
public:
    plugin(const char* filename);
    ~plugin();

    bool valid() const { return handle_ != NULL; }
    const std::string& file() const { return file_; }

    void open(plugin_binding*);
    void close(plugin_binding*);
    void on_sync_point(plugin_binding*, const sync_point*);
    void on_hardware_access(plugin_binding*, const saved_hardware_access*, const unsigned char*);
    void on_command(plugin_binding*);

    bool operator==(const plugin& other) const;

private:
    void* load_symbol(const char* name);

    std::string file_;

    open_plugin_t open_plugin_callback_;
    close_plugin_t close_plugin_callback_;
    sync_point_callback_t sync_point_callback_;
    hardware_access_callback_t hardware_access_callback_;
    command_callback_t command_callback_;

    void* handle_;
};

// forward declare vm_tweaks
class vm_tweaks;

//! Plugin mechanism interface
class plugin_manager
{
public:
    plugin_manager(vm_tweaks* t);
    ~plugin_manager();

    void load_plugin(const char* path);

    void sync_point_callback(const sync_point* sp);
    void hardware_access_callback(const saved_hardware_access* access, const unsigned char* data);
    void command_callback();

private:
    typedef std::vector<plugin> plugin_vector;

    plugin_vector::iterator find_plugin(const plugin& p);

    plugin_vector plugins_;

    plugin_binding binding_;
};

}} // namespace tetrane::tweaks

#endif
