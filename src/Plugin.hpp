#pragma once

#include <windows.h>

#include <memory>
#include "uevr/API.hpp"
#include "uevr/Plugin.hpp"
#include <glm/glm.hpp>
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
    void on_pre_engine_tick(uevr::API::UGameEngine* engine, float delta) override;

private:
    void hook_get_spread_shoot_vector();
    void hook_melee_trace_check();

    void* on_get_end_trace_loc_internal(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float sangles, glm::f64vec3* out_vec);
    static void* on_get_end_trace_loc(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float sangles, glm::f64vec3* out_vec) {
        return g_plugin->on_get_end_trace_loc_internal(weapon, in_angles, sangles, out_vec);
    }

    glm::f64vec3* on_get_trace_start_loc_internal(uevr::API::UObject* weapon, glm::f64vec3* out_vec);
    static glm::f64vec3* on_get_trace_start_loc(uevr::API::UObject* weapon, glm::f64vec3* out_vec) {
        return g_plugin->on_get_trace_start_loc_internal(weapon, out_vec);
    }

    bool m_hooked{false};
    //safetyhook::InlineHook m_on_get_end_trace_loc_hook;
    //safetyhook::InlineHook m_trace_start_loc_hook;
    int32_t m_on_get_end_trace_loc_hook_id{};
    int32_t m_trace_start_loc_hook_id{};
    using GetEndTraceLocFn = void*(*)(uevr::API::UObject*, glm::f64vec2*, float, glm::f64vec3*);
    GetEndTraceLocFn m_on_get_end_trace_loc_hook_fn{};
    using GetStartTraceLocFn = glm::f64vec3*(*)(uevr::API::UObject*, glm::f64vec3*);
    GetStartTraceLocFn m_trace_start_loc_hook_fn{};

    int32_t m_melee_trace_check_hook_id{};
    using MeleeTraceCheckFn = bool (*)(void*, float, float, float, void*, void*, void*);
    MeleeTraceCheckFn m_melee_trace_check_hook_fn{};
    bool on_melee_trace_check_internal(void*, float, float, float, void*, void*, void*);
    static bool on_melee_trace_check(void* a1, float a2, float a3, float a4, void* a5, void* a6, void* a7) {
        return g_plugin->on_melee_trace_check_internal(a1, a2, a3, a4, a5, a6, a7);
    }
};