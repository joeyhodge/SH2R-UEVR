#pragma once

#include <windows.h>

#include <memory>
#include "uevr/API.hpp"
#include "uevr/Plugin.hpp"
#include <glm/glm.hpp>
#include <safetyhook.hpp>
#include <utility/PointerHook.hpp>

#define PLUGIN_LOG_ONCE(...) { \
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_info(__VA_ARGS__); \
    } }

#define PLUGIN_LOG_ONCE_ERROR(...) { \
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_error(__VA_ARGS__); \
    } }

// Global accessor for our plugin.
class SHPlugin;
extern std::unique_ptr<SHPlugin> g_plugin;

class SHPlugin : public uevr::Plugin {
public:
    SHPlugin() = default;
    virtual ~SHPlugin();

    void on_initialize() override;

private:
    void hook_get_spread_shoot_vector();

    void* on_get_spread_shoot_vector_internal(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float sangles, glm::f64vec3* out_vec);
    static void* on_get_spread_shoot_vector(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float sangles, glm::f64vec3* out_vec) {
        return g_plugin->on_get_spread_shoot_vector_internal(weapon, in_angles, sangles, out_vec);
    }

    glm::f64vec3* on_get_trace_start_loc_internal(uevr::API::UObject* weapon, glm::f64vec3* out_vec);
    static glm::f64vec3* on_get_trace_start_loc(uevr::API::UObject* weapon, glm::f64vec3* out_vec) {
        return g_plugin->on_get_trace_start_loc_internal(weapon, out_vec);
    }

    bool m_hooked{false};
    safetyhook::InlineHook m_spread_shoot_vector_hook;
    safetyhook::InlineHook m_trace_start_loc_hook;
};