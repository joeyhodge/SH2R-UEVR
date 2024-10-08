#include <utility/Scan.hpp>

#include "Plugin.hpp"

using namespace uevr;

std::unique_ptr<SHPlugin> g_plugin = std::make_unique<SHPlugin>();

SHPlugin::~SHPlugin() {

}

void SHPlugin::on_initialize() {
    PLUGIN_LOG_ONCE("SHPlugin::on_initialize()");

    hook_get_spread_shoot_vector();
}

safetyhook::InlineHook hook_vtable_fn(std::wstring_view class_name, std::wstring_view fn_name, uintptr_t destination) {
    auto WeaponPistol_BP_C = (API::UClass*)API::get()->find_uobject(class_name);

    if (WeaponPistol_BP_C == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find %ls", class_name.data());
        return safetyhook::InlineHook();
    }

    auto fn = WeaponPistol_BP_C->find_function(fn_name);

    if (fn == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find %ls", fn_name.data());
        return safetyhook::InlineHook();
    }

    auto native = fn->get_native_function();

    if (native == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to get native function");
        return safetyhook::InlineHook();
    }

    //PLUGIN_LOG_ONCE("%ls native: 0x%p", fn_name.data(), native);

    auto default_object = WeaponPistol_BP_C->get_class_default_object();

    if (default_object == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to get default object");
        return safetyhook::InlineHook();
    }

    auto insn = utility::scan_disasm((uintptr_t)native, 0x1000, "FF 90 ? ? ? ?");

    if (!insn) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find the instruction");
        return safetyhook::InlineHook();
    }

    auto offset = *(int32_t*)(*insn + 2);

    auto vtable = *(uintptr_t**)default_object;
    auto real_fn = vtable[offset / sizeof(void*)];

    //PLUGIN_LOG_ONCE("Real %ls: 0x%p (index: %d, offset 0x%X)", fn_name.data(), real_fn, offset / sizeof(void*), offset);

    auto result = safetyhook::create_inline(real_fn, destination);

    //PLUGIN_LOG_ONCE("Hooked %ls", fn_name.data());

    return std::move(result);
}

// This is one thing we can't do from Lua,
// because well... it's not called from BP.
// It also calls a virtual function internally which we need to find the index of.
void SHPlugin::hook_get_spread_shoot_vector() {
    m_spread_shoot_vector_hook = hook_vtable_fn(L"Class /Script/SHProto.SHItemWeaponRanged", L"GetEndTraceLoc", (uintptr_t)on_get_spread_shoot_vector);
    m_trace_start_loc_hook = hook_vtable_fn(L"Class /Script/SHProto.SHItemWeaponRanged", L"GetStartTraceLoc", (uintptr_t)on_get_trace_start_loc);
}

glm::f64vec3* SHPlugin::on_get_trace_start_loc_internal(uevr::API::UObject* weapon, glm::f64vec3* out_vec) {
    auto result = m_trace_start_loc_hook.unsafe_call<glm::f64vec3*>(weapon, out_vec);

    if (result != nullptr) {
        //API::get()->log_info("Start Trace Current result: %f, %f, %f", result->x, result->y, result->z);

        auto fire_point_component = weapon->get_property<API::UObject*>(L"FirePoint");

        if (fire_point_component != nullptr) {
            struct {
                glm::f64vec3 location;
            } location_params;
            fire_point_component->call_function(L"K2_GetComponentLocation", &location_params);

            result->x = location_params.location.x;
            result->y = location_params.location.y;
            result->z = location_params.location.z;
        }
    }

    return result;
}

void* SHPlugin::on_get_spread_shoot_vector_internal(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float shootangles, glm::f64vec3* out_vec) {
    API::get()->log_info("SHPlugin::on_get_spread_shoot_vector_internal(0x%p, %f, %f, %f, 0x%p)", weapon, in_angles->x, in_angles->y, out_vec);

    // Call the original function
    void* result = m_spread_shoot_vector_hook.unsafe_call<void*>(weapon, in_angles, shootangles, out_vec);

    // Modify the output vector
    /*if (out_vec != nullptr) {
        API::get()->log_info("Current out_vec: %f, %f, %f", (float)out_vec->x, (float)out_vec->y, (float)out_vec->z);
        out_vec->x = 0.0;
        out_vec->y = 0.0;
        out_vec->z = 0.0;
    }*/

    const auto result_f64vec3 = (glm::f64vec3*)result;

    if (result_f64vec3 != nullptr) {
        API::get()->log_info("Current result: %f, %f, %f", result_f64vec3->x, result_f64vec3->y, result_f64vec3->z);

        // Get the direction the muzzle is facing instead
        auto fire_point_component = weapon->get_property<API::UObject*>(L"FirePoint");
        auto root_component = weapon->get_property<API::UObject*>(L"RootComponent");

        if (fire_point_component != nullptr && root_component != nullptr) {
            struct {
                glm::f64vec3 location;
            } location_params;
            fire_point_component->call_function(L"K2_GetComponentLocation", &location_params);

            struct {
                glm::f64vec3 forward;
            } forward_params;
            //fire_point_component->call_function(L"GetForwardVector", &forward_params);
            root_component->call_function(L"GetForwardVector", &forward_params);

            API::get()->log_info("FirePoint location: %f, %f, %f", location_params.location.x, location_params.location.y, location_params.location.z);
            API::get()->log_info("FirePoint forward: %f, %f, %f", forward_params.forward.x, forward_params.forward.y, forward_params.forward.z);

            // Calculate the new vector
            auto new_vec = location_params.location + (forward_params.forward * 8192.0);

            result_f64vec3->x = new_vec.x;
            result_f64vec3->y = new_vec.y;
            result_f64vec3->z = new_vec.z;
        }
    } 

    return result;
}