
#include <algorithm>
#include <dlfcn.h>
#include <iprt/assert.h>
#include "plugin.h"
#include "vm_tweaks.h"

namespace tetrane {
namespace tweaks {

namespace {
extern "C" {

int write_core(plugin_binding* p, const char* name)
{
    vm_tweaks* t = p->impl;
    t->write_core(name);
    return 0;
}

uint64_t read_tsc(plugin_binding* p)
{
    vm_tweaks* t = p->impl;
    return t->read_tsc();
}

} // extern "C"
} // anonymous namespace

plugin::plugin(const char* filename)
    : file_(filename)
    , sync_point_callback_(NULL)
    , hardware_access_callback_(NULL)
    , command_callback_(NULL)
{
    handle_ = dlopen(filename, RTLD_NOW);
    if (handle_) {
        open_plugin_callback_ = reinterpret_cast<open_plugin_t>(load_symbol(OPEN_PLUGIN_CALLBACK_NAME));
        close_plugin_callback_ = reinterpret_cast<close_plugin_t>(load_symbol(CLOSE_PLUGIN_CALLBACK_NAME));
        sync_point_callback_ = reinterpret_cast<sync_point_callback_t>(load_symbol(SYNC_POINT_CALLBACK_NAME));
        hardware_access_callback_ = reinterpret_cast<hardware_access_callback_t>(load_symbol(HW_ACCESS_CALLBACK_NAME));
        command_callback_ = reinterpret_cast<command_callback_t>(load_symbol(COMMAND_CALLBACK_NAME));
    }
}

plugin::~plugin()
{
    // I decide to leak (man dlclose) the plugin handle because:
    // - We can't just close the handle in the destructor as it'll imply a non-copyable plugin object which is not
    //   supported by c++98 vectors.
    // - Because of the low (actually one) number of loaded plugin, it's not worth reimplementing a reference
    //   counting mechanism.
}

void plugin::open(plugin_binding* b)
{
    Assert(valid());
    if (open_plugin_callback_) {
        open_plugin_callback_(b);
    }
}

void plugin::close(plugin_binding* b)
{
    Assert(valid());
    if (close_plugin_callback_) {
        close_plugin_callback_(b);
    }

    dlclose(handle_);
}

void plugin::on_sync_point(plugin_binding* b, const sync_point* p)
{
    Assert(valid());
    if (sync_point_callback_) {
        sync_point_callback_(b, p);
    }
}

void plugin::on_hardware_access(plugin_binding* b, const saved_hardware_access* access, const unsigned char* data)
{
    Assert(valid());
    if (hardware_access_callback_) {
        hardware_access_callback_(b, access, data);
    }
}

void plugin::on_command(plugin_binding* b)
{
    Assert(valid());
    if (command_callback_)
        command_callback_(b);
}

bool plugin::operator==(const plugin& other) const
{
    return valid() and (handle_ == other.handle_);
}

void* plugin::load_symbol(const char* name)
{
    Assert(valid());
    dlerror(); // clear pending errors

    void* sym = dlsym(handle_, name);
    if (!sym) {
        std::cerr << "symbol not found: " << dlerror() << std::endl;
    }
    return sym;
}


plugin_manager::plugin_manager(vm_tweaks* t)
{
    binding_.write_core = &write_core;
    binding_.read_tsc = &read_tsc;
    binding_.impl = t;
}

plugin_manager::~plugin_manager()
{
    plugin_vector::iterator it = plugins_.begin();
    while (it != plugins_.end()) {
        it->close(&binding_);
        it = plugins_.erase(it);
    }
}

void plugin_manager::load_plugin(const char* file)
{
    plugin p(file);

    if (!p.valid()) {
        std::cerr << "cannot load plugin " << p.file() << std::endl;
        return;
    }

    if (find_plugin(p) != plugins_.end()) {
        std::cerr << "plugin " << p.file() << " already loaded" << std::endl;
        return;
    }

    plugin_vector::iterator it = plugins_.insert(plugins_.begin(), p);

    it->open(&binding_);
}

void plugin_manager::sync_point_callback(const sync_point* sp)
{
    plugin_vector::iterator end = plugins_.end();
    for (plugin_vector::iterator it=plugins_.begin(); it != end; ++it) {
        it->on_sync_point(&binding_, sp);
    }
}

void plugin_manager::hardware_access_callback(const saved_hardware_access* access, const unsigned char* data)
{
    plugin_vector::iterator end = plugins_.end();
    for (plugin_vector::iterator it=plugins_.begin(); it != end; ++it) {
        it->on_hardware_access(&binding_, access, data);
    }
}

void plugin_manager::command_callback()
{
    plugin_vector::iterator end = plugins_.end();
    for (plugin_vector::iterator it=plugins_.begin(); it != end; ++it) {
        it->on_command(&binding_);
    }
}

plugin_manager::plugin_vector::iterator plugin_manager::find_plugin(const plugin& p)
{
    return std::find(plugins_.begin(), plugins_.end(), p);
}

}} // namespace tetrane::tweaks
