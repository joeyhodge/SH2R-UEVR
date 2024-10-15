#include <iostream>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/String.hpp>

#include "Plugin.hpp"

using namespace uevr;

std::unique_ptr<SHPlugin> g_plugin = std::make_unique<SHPlugin>();

SHPlugin::~SHPlugin() {

}

void SHPlugin::on_initialize() {
    PLUGIN_LOG_ONCE("SHPlugin::on_initialize()");

    hook_get_spread_shoot_vector();
    hook_melee_trace_check();
}

void SHPlugin::on_pre_engine_tick(uevr::API::UGameEngine* engine, float delta) {

}

int32_t hook_vtable_fn(std::wstring_view class_name, std::wstring_view fn_name, void* destination, void** original) {
    auto obj = (API::UClass*)API::get()->find_uobject(class_name);

    if (obj == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find %ls", class_name.data());
        return -1;
    }

    auto fn = obj->find_function(fn_name);

    if (fn == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find %ls", fn_name.data());
        return -1;
    }

    auto native = fn->get_native_function();

    if (native == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to get native function");
        return -1;
    }

    //PLUGIN_LOG_ONCE("%ls native: 0x%p", fn_name.data(), native);

    auto default_object = obj->get_class_default_object();

    if (default_object == nullptr) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to get default object");
        return -1;
    }

    auto insn = utility::scan_disasm((uintptr_t)native, 0x1000, "FF 90 ? ? ? ?");

    if (!insn) {
        //PLUGIN_LOG_ONCE_ERROR("Failed to find the instruction");
        return -1;
    }

    auto offset = *(int32_t*)(*insn + 2);

    auto vtable = *(uintptr_t**)default_object;
    auto real_fn = vtable[offset / sizeof(void*)];

    //PLUGIN_LOG_ONCE("Real %ls: 0x%p (index: %d, offset 0x%X)", fn_name.data(), real_fn, offset / sizeof(void*), offset);

    return API::get()->param()->functions->register_inline_hook((void*)real_fn, (void*)destination, original);
}

// This is one thing we can't do from Lua,
// because well... it's not called from BP.
// It also calls a virtual function internally which we need to find the index of.
void SHPlugin::hook_get_spread_shoot_vector() {
    m_on_get_end_trace_loc_hook_id = hook_vtable_fn(L"Class /Script/SHProto.SHItemWeaponRanged", L"GetEndTraceLoc", on_get_end_trace_loc, (void**)&m_on_get_end_trace_loc_hook_fn);
    m_trace_start_loc_hook_id = hook_vtable_fn(L"Class /Script/SHProto.SHItemWeaponRanged", L"GetStartTraceLoc", on_get_trace_start_loc, (void**)&m_trace_start_loc_hook_fn);
}

// Another one that most definitely cannot be done in Lua without FFI.
void SHPlugin::hook_melee_trace_check() {
    PLUGIN_LOG_ONCE("SHPlugin::hook_melee_trace_check()");

    const auto game = utility::get_executable();
    auto fn = utility::find_function_from_string_ref(game, "MeleeWeaponEnvTrace");

    if (!fn) {
        PLUGIN_LOG_ONCE_ERROR("Failed to find MeleeWeaponEnvTrace");
        return;
    }

    fn = utility::find_function_start_with_call(*fn); // Looks for E8 call to the function to locate the start

    if (!fn) {
        PLUGIN_LOG_ONCE_ERROR("Failed to find MeleeWeaponEnvTrace start");
        return;
    }

    PLUGIN_LOG_ONCE("MeleeWeaponEnvTrace: 0x%p", (void*)*fn);

    m_melee_trace_check_hook_id = API::get()->param()->functions->register_inline_hook((void*)*fn, (void*)on_melee_trace_check, (void**)&m_melee_trace_check_hook_fn);

    if (m_melee_trace_check_hook_id == -1) {
        PLUGIN_LOG_ONCE_ERROR("Failed to hook MeleeWeaponEnvTrace");
        return;
    }

    PLUGIN_LOG_ONCE("Hooked MeleeWeaponEnvTrace");
}

bool SHPlugin::on_melee_trace_check_internal(API::UObject* melee_item, float a2, float a3, float a4, void* a5, void* a6, uevr::API::TArray<SHPlugin::FHitResult>& hit_results) {
    PLUGIN_LOG_ONCE("SHPlugin::on_melee_trace_check_internal(0x%p, %f, %f, %f, 0x%p, 0x%p, %d)", melee_item, a2, a3, a4, a5, a6, hit_results.count);

    if (melee_item == nullptr) {
        return false;
    }

    if (!API::get()->param()->vr->is_using_controllers()) {
        // Just do what the game originally wanted to do
        return m_melee_trace_check_hook_fn(melee_item, a2, a3, a4, a5, a6, hit_results);
    }

    // Let's try and do it ourselves
    static const auto kismet_system_library_c = API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.KismetSystemLibrary");
    static const auto kismet_system_library = kismet_system_library_c->get_class_default_object();

    static auto kismet_math_library_c = API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.KismetMathLibrary");
    static auto kismet_math_library = kismet_math_library_c->get_class_default_object();

    struct {
        API::UObject* world_context_object{};
        glm::f64vec3 start{};
        glm::f64vec3 end{};
        float radius{1.0f};
        float half_height{1.0f};
        uint8_t trace_channel{3};
        bool trace_complex{true};
        API::TArray<API::UObject*> actors_to_ignore{};
        uint8_t draw_debug_type{0};
        //FHitResult hit_result{};
        API::TArray<FHitResult> hit_results{}; // For multi variant
        bool ignore_self;
        float trace_color[4]{1.0f, 0.0f, 0.0f, 1.0f};
        float trace_hit_color[4]{0.0f, 1.0f, 0.0f, 1.0f};
        float draw_time{1.0f};
        bool return_value{false};
    } params;

    auto engine = API::get()->get_engine();
    auto viewport = engine != nullptr ? engine->get_property<API::UObject*>(L"GameViewport") : nullptr;
    auto world = viewport != nullptr ? viewport->get_property<API::UObject*>(L"World") : nullptr;

    params.world_context_object = world;
    params.actors_to_ignore.data = (API::UObject**)API::FMalloc::get()->malloc(2 * sizeof(API::UObject*));
    params.actors_to_ignore.count = 2;
    params.actors_to_ignore.capacity = 2;
    params.actors_to_ignore.data[0] = melee_item;
    params.actors_to_ignore.data[1] = API::get()->get_local_pawn(0);
    params.ignore_self = true;

    auto mesh_component = melee_item->get_property<API::UObject*>(L"Mesh");

    struct Transform {
        char padding[0x60]; // We do not care what's in here  
    } mesh_transform;
    static_assert(sizeof(Transform) == 0x60, "Transform size mismatch");

    struct BoxSphereBounds {
        glm::f64vec3 origin{};
        glm::f64vec3 box_extent{};
        double sphere_radius{};
    } mesh_bounds;
    static_assert(sizeof(BoxSphereBounds) == 0x38, "BoxSphereBounds size mismatch");

    if (mesh_component != nullptr) {
        mesh_component->call_function(L"K2_GetComponentToWorld", &mesh_transform);

        auto skeletal_mesh = mesh_component->get_property<API::UObject*>(L"SkeletalMesh");

        if (skeletal_mesh != nullptr) {
            skeletal_mesh->call_function(L"GetBounds", &mesh_bounds);

            int32_t largest_axis{0};
            double largest_extent{0.0};

            for (int32_t i = 0; i < 3; i++) {
                if (glm::abs(mesh_bounds.box_extent[i]) > largest_extent) {
                    largest_extent = glm::abs(mesh_bounds.box_extent[i]);
                    largest_axis = i;
                }
            }

            struct {
                Transform transform;
                glm::f64vec3 location;
                glm::f64vec3 result;
            } transform_location_params;

            transform_location_params.transform = mesh_transform;
            transform_location_params.location = mesh_bounds.origin;
            transform_location_params.location[largest_axis] -= mesh_bounds.box_extent[largest_axis]; // Lengthwise along the mesh

            kismet_math_library->call_function(L"TransformLocation", &transform_location_params);

            params.start = transform_location_params.result;

            transform_location_params.location = mesh_bounds.origin;
            transform_location_params.location[largest_axis] += mesh_bounds.box_extent[largest_axis]; // Lengthwise along the mesh

            kismet_math_library->call_function(L"TransformLocation", &transform_location_params);

            glm::f64vec3 short_axes{mesh_bounds.box_extent};
            short_axes[largest_axis] = 0.0;

            double biggest_short_axis{0.0};

            for (int32_t i = 0; i < 3; i++) {
                if (glm::abs(short_axes[i]) > biggest_short_axis) {
                    biggest_short_axis = glm::abs(short_axes[i]);
                }
            }

            params.end = transform_location_params.result;
            params.radius = (float)biggest_short_axis;
            params.half_height = (float)(mesh_bounds.box_extent[largest_axis] / 2.0);
            //params.half_height = 0.001f;
        }
    }

    //kismet_system_library->call_function(L"LineTraceSingle", &params);
    //kismet_system_library->call_function(L"SphereTraceSingle", &params);

    auto add_uobject_to_tarray = [](API::TArray<API::UObject*>& array, API::UObject* obj) {
        if (array.count >= array.capacity) {
            size_t new_capacity = std::max(array.capacity * 2, 2);

            if (array.data != nullptr) {
                array.data = (API::UObject**)API::FMalloc::get()->realloc(array.data, new_capacity * sizeof(API::UObject*));
            } else {
                array.data = (API::UObject**)API::FMalloc::get()->malloc(new_capacity * sizeof(API::UObject*));
            }

            array.capacity = new_capacity;
        }

        array.data[array.count++] = obj;
    };

    bool any_succeed = false;

    for (size_t j = 0; j < 2; ++j) {
        if (any_succeed) {
            //break;
        }

        if (j == 1) {
            std::swap(params.start, params.end); // For some reason the game might only hit enemies if the impact hits a certain way
        }

        // Do a few traces, we should hit multiple actors/components.
        kismet_system_library->call_function(L"CapsuleTraceMulti", &params);

        for (size_t i = 0; i < params.hit_results.count; i++) {
            auto& hit_result = params.hit_results.data[i];
            const bool result = hit_result.bBlockingHit && params.return_value;

            if (result) {
                if (hit_result.HitObject_ActorIndex > 0) {
                    auto actor_item = API::FUObjectArray::get()->get_item(hit_result.HitObject_ActorIndex);

                    if (actor_item != nullptr) {
                        auto actor = actor_item->object;

                        if (actor != nullptr) {
                            API::get()->log_info("Hit actor: %s", utility::narrow(actor->get_full_name()).c_str());
                            std::cout << "Hit actor: " << utility::narrow(actor->get_full_name()) << std::endl;

                            //add_uobject_to_tarray(params.actors_to_ignore, actor);
                        }
                    }

                    auto component_item = API::FUObjectArray::get()->get_item(hit_result.Component_Index);

                    if (component_item != nullptr) {
                        auto component = component_item->object;

                        if (component != nullptr) {
                            API::get()->log_info("Hit component: %s", utility::narrow(component->get_full_name()).c_str());
                            std::cout << "Hit component: " << utility::narrow(component->get_full_name()) << std::endl;

                            /*static const auto capsule_component_c = API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.CapsuleComponent");

                            if (component->is_a(capsule_component_c)) {
                                continue; // We don't want to hit capsule components
                            }*/

                            static const auto mesh_component_c = API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.MeshComponent");

                            if (!component->is_a(mesh_component_c)) {
                                continue; // We only want to hit mesh components
                            }
                        }
                    }
                }

                if (hit_results.count >= hit_results.capacity) {
                    size_t new_capacity = std::max(hit_results.capacity * 2, 2);

                    if (hit_results.data != nullptr) {
                        hit_results.data = (FHitResult*)API::FMalloc::get()->realloc(hit_results.data, new_capacity * sizeof(FHitResult));
                    } else {
                        hit_results.data = (FHitResult*)API::FMalloc::get()->malloc(new_capacity * sizeof(FHitResult));
                    }

                    hit_results.capacity = new_capacity;
                }

                memcpy(&hit_results.data[hit_results.count++], &hit_result, sizeof(FHitResult));

                any_succeed = true;
            }
        }
    }

    if (any_succeed) {
        API::get()->dispatch_lua_event("OnMeleeTraceSuccess", "");
    }

    return any_succeed;
}

glm::f64vec3* SHPlugin::on_get_trace_start_loc_internal(uevr::API::UObject* weapon, glm::f64vec3* out_vec) {
    auto result = m_trace_start_loc_hook_fn(weapon, out_vec);

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

            //API::get()->dispatch_lua_event("GetStartTraceLoc", "Test");
        }
    }

    return result;
}

void* SHPlugin::on_get_end_trace_loc_internal(uevr::API::UObject* weapon, glm::f64vec2* in_angles, float shootangles, glm::f64vec3* out_vec) {
    //API::get()->log_info("SHPlugin::on_get_spread_shoot_vector_internal(0x%p, %f, %f, %f, 0x%p)", weapon, in_angles->x, in_angles->y, out_vec);

    // Call the original function
    void* result = m_on_get_end_trace_loc_hook_fn(weapon, in_angles, shootangles, out_vec);

    // Modify the output vector
    /*if (out_vec != nullptr) {
        API::get()->log_info("Current out_vec: %f, %f, %f", (float)out_vec->x, (float)out_vec->y, (float)out_vec->z);
        out_vec->x = 0.0;
        out_vec->y = 0.0;
        out_vec->z = 0.0;
    }*/

    const auto result_f64vec3 = (glm::f64vec3*)result;

    if (result_f64vec3 != nullptr) {
        //API::get()->log_info("Current result: %f, %f, %f", result_f64vec3->x, result_f64vec3->y, result_f64vec3->z);

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

            const auto name = weapon->get_fname()->to_string();

            // Rifles and shotguns have their muzzle facing the wrong way (90 degrees off)
            // so we have to correct it
            if (name.contains(L"Rifle") or name.contains(L"Shotgun")) {
                struct {
                    glm::f64vec3 location;
                    float angle;
                    glm::f64vec3 axis;
                    glm::f64vec3 result;
                } rotate_params;
                static auto kismet_math_library_c = API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.KismetMathLibrary");
                static auto kismet_math_library = kismet_math_library_c->get_class_default_object();

                rotate_params.location = forward_params.forward;
                rotate_params.angle = 90.0f;
                root_component->call_function(L"GetUpVector", &rotate_params.axis);
                kismet_math_library->call_function(L"RotateAngleAxis", &rotate_params);

                forward_params.forward = rotate_params.result;
            }
            //API::get()->log_info("FirePoint location: %f, %f, %f", location_params.location.x, location_params.location.y, location_params.location.z);
            //API::get()->log_info("FirePoint forward: %f, %f, %f", forward_params.forward.x, forward_params.forward.y, forward_params.forward.z);

            // Calculate the new vector
            auto new_vec = location_params.location + (forward_params.forward * 8192.0);

            result_f64vec3->x = new_vec.x;
            result_f64vec3->y = new_vec.y;
            result_f64vec3->z = new_vec.z;

            //API::get()->dispatch_lua_event("GetEndTraceLoc", "Test");
        }
    } 

    return result;
}