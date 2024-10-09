local function find_required_object(name)
    local obj = uevr.api:find_uobject(name)
    if not obj then
        error("Cannot find " .. name)
        return nil
    end

    return obj
end

local SHCharacterPlayCameraComponent_c = find_required_object("Class /Script/SHProto.SHCharacterPlayCameraComponent")
local SHGameplaySaveMenuWidget_c = find_required_object("Class /Script/SHProto.SHGameplaySaveMenuWidget")
local SHCharAnimationInstance_c = find_required_object("AnimBlueprintGeneratedClass /Game/Game/Characters/Humans/JamesSunderland/Animation/AnimationBlueprints/CH_JamesAnimBP.CH_JamesAnimBP_C")

local AnimNode_Fabrik = find_required_object("ScriptStruct /Script/AnimGraphRuntime.AnimNode_Fabrik")

print(string.format("0x%llx", AnimNode_Fabrik:get_class_default_object():get_address()))

local api = uevr.api

local BlueprintUpdateAnimation = SHCharAnimationInstance_c:find_function("BlueprintUpdateAnimation")

BlueprintUpdateAnimation:hook_ptr(nil, function(fn, obj, locals, result)
    obj.UseWaponIK = false
end)

local find_static_class = function(name)
    local c = api:find_uobject(name)
    if not c then
        error("Cannot find class " .. name)
        return nil
    end

    return c:get_class_default_object()
end

local SHCharacterStatics = find_static_class("Class /Script/SHProto.SHCharacterStatics")
local Statics = find_static_class("Class /Script/Engine.GameplayStatics")

if not SHCharacterStatics then
    error("Cannot find SHCharacterStatics")
    return
end

if not Statics then
    error("Cannot find Statics")
    return
end

local SHCrosshairWidget_c = find_required_object("Class /Script/SHProto.SHCrosshairWidget")
local hitresult_c = find_required_object("ScriptStruct /Script/Engine.HitResult")
local empty_hitresult = StructObject.new(hitresult_c)
local reusable_hit_result = StructObject.new(hitresult_c)

local function find_camera_component()
    local components = SHCharacterPlayCameraComponent_c:get_objects_matching(false)
    if components == nil or #components == 0 then
        return nil
    end

    for _, component in ipairs(components) do
        if component.OwnerCharacter ~= nil then
            return component
        end
    end

    return nil
end

local function find_save_menu_widget()
    local widgets = SHGameplaySaveMenuWidget_c:get_objects_matching(false)
    if widgets == nil or #widgets == 0 then
        return nil
    end

    for _, widget in ipairs(widgets) do
        if widget.OwnerCharacter ~= nil then
            return widget
        end
    end

    return nil
end

local function is_save_menu_open()
    local widget = find_save_menu_widget()
    if widget == nil then
        return false
    end

    return widget:IsVisible() and widget:IsRendered()
end

local last_rot = nil

local kismet_string_library = find_static_class("Class /Script/Engine.KismetStringLibrary")
local kismet_math_library = find_static_class("Class /Script/Engine.KismetMathLibrary")
local kismet_system_library = find_static_class("Class /Script/Engine.KismetSystemLibrary")
if not kismet_string_library then
    error("Cannot find KismetStringLibrary")
    return
end

if not kismet_math_library then
    error("Cannot find KismetMathLibrary")
    return
end

if not kismet_system_library then
    error("Cannot find KismetSystemLibrary")
    return
end

local SHAnimIKHandIKSubcomp_c = api:find_uobject("Class /Script/SHProto.SHAnimHandIKSubcomp")

if not SHAnimIKHandIKSubcomp_c then
    error("Cannot find SHAnimIKHandIKSubcomp")
    return
end

local game_engine_class = find_required_object("Class /Script/Engine.GameEngine")
local widget_component_c = find_required_object("Class /Script/UMG.WidgetComponent")
local scene_component_c = find_required_object("Class /Script/Engine.SceneComponent")
local actor_c = find_required_object("Class /Script/Engine.Actor")
local ftransform_c = find_required_object("ScriptStruct /Script/CoreUObject.Transform")
local temp_transform = StructObject.new(ftransform_c)

local color_c = find_required_object("ScriptStruct /Script/CoreUObject.LinearColor")
local zero_color = StructObject.new(color_c)

local temp_actor = nil
local left_hand_actor = nil
local right_hand_actor = nil
local left_hand_component = nil
local right_hand_component = nil


local function spawn_actor(world_context, actor_class, location, collision_method, owner)
    temp_transform.Translation = location
    temp_transform.Rotation.W = 1.0
    temp_transform.Scale3D = Vector3d.new(1.0, 1.0, 1.0)

    local actor = Statics:BeginDeferredActorSpawnFromClass(world_context, actor_class, temp_transform, collision_method, owner)

    if actor == nil then
        print("Failed to spawn actor")
        return nil
    end

    Statics:FinishSpawningActor(actor, temp_transform)

    return actor
end

local function reset_temp_actor()
    if temp_actor ~= nil and UEVR_UObjectHook.exists(temp_actor) then
        temp_actor:K2_DestroyActor()
    end

    temp_actor = nil
end

local function reset_temp_actor_if_deleted()
    if temp_actor ~= nil and not UEVR_UObjectHook.exists(temp_actor) then
        temp_actor = nil
    end
end

local function setup_crosshair_actor(pawn, crosshair_widget)
    local game_engine = UEVR_UObjectHook.get_first_object_by_class(game_engine_class)

    local viewport = game_engine.GameViewport
    if viewport == nil then
        print("Viewport is nil")
        return
    end

    local world = viewport.World
    if world == nil then
        print("World is nil")
        return
    end

    reset_temp_actor()

    local pos = pawn:K2_GetActorLocation()
    temp_actor = spawn_actor(world, actor_c, pos, 1, nil)

    if temp_actor == nil then
        print("Failed to spawn actor")
        return
    end

    print("Spawned actor")

    temp_transform.Translation = pos
    temp_transform.Rotation.W = 1.0
    temp_transform.Scale3D = Vector3d.new(1.0, 1.0, 1.0)
    local widget_component = temp_actor:AddComponentByClass(widget_component_c, false, temp_transform, false)

    if widget_component == nil then
        print("Failed to add widget component")
        return
    end

    print("Added widget component")

    -- Add crosshair widget to the widget component
    crosshair_widget:RemoveFromViewport()


    --local wanted_mat_name = "Material /Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"
    local wanted_mat_name = "Material /Engine/EngineMaterials/GizmoMaterial.GizmoMaterial"
    local wanted_mat = api:find_uobject(wanted_mat_name)

    widget_component:SetWidget(crosshair_widget)
    widget_component:SetVisibility(true)
    widget_component:SetHiddenInGame(false)
    widget_component:SetCollisionEnabled(0)

    --crosshair_widget:AddToViewport(0)

    widget_component:SetRenderCustomDepth(true)
    widget_component:SetCustomDepthStencilValue(100)
    widget_component:SetCustomDepthStencilWriteMask(1)
    widget_component:SetRenderInDepthPass(false)

    if wanted_mat then
        wanted_mat.bDisableDepthTest = true
        wanted_mat.BlendMode = 2
        --wanted_mat.MaterialDomain = 0
        --wanted_mat.ShadingModel = 0
        widget_component:SetMaterial(1, wanted_mat)
    end
    
    widget_component.BlendMode = 2

    temp_actor:FinishAddComponent(widget_component, false, temp_transform)
    widget_component:SetWidget(crosshair_widget)

    -- Disable depth testing
    --[[widget_component:SetRenderCustomDepth(true)
    widget_component:SetCustomDepthStencilValue(100)
    widget_component:SetCustomDepthStencilWriteMask(1)
    widget_component:SetRenderCustomDepth(true)
    widget_component:SetRenderInDepthPass(false)]]
    --widget_component.BlendMode = 2
    widget_component:SetTwoSided(true)


    print("Widget space: " .. tostring(widget_component.Space))
    print("Widget draw size: X=" .. widget_component.DrawSize.X .. ", Y=" .. widget_component.DrawSize.Y)
    print("Widget visibility: " .. tostring(widget_component:IsVisible()))
end

local function reset_hand_actors()
    if left_hand_actor ~= nil then
        left_hand_actor:K2_DestroyActor()
        left_hand_actor = nil
    end

    if right_hand_actor ~= nil then
        right_hand_actor:K2_DestroyActor()
        right_hand_actor = nil
    end
end

local function reset_hand_actors_if_deleted()
    if left_hand_actor ~= nil and not UEVR_UObjectHook.exists(left_hand_actor) then
        left_hand_actor = nil
    end

    if right_hand_actor ~= nil and not UEVR_UObjectHook.exists(right_hand_actor) then
        right_hand_actor = nil
    end
end

local function spawn_hand_actors()
    local game_engine = UEVR_UObjectHook.get_first_object_by_class(game_engine_class)

    local viewport = game_engine.GameViewport
    if viewport == nil then
        print("Viewport is nil")
        return
    end

    local world = viewport.World
    if world == nil then
        print("World is nil")
        return
    end

    reset_hand_actors()

    local pawn = api:get_local_pawn(0)

    if pawn == nil then
        print("Pawn is nil")
        return
    end

    local pos = pawn:K2_GetActorLocation()

    left_hand_actor = spawn_actor(world, actor_c, pos, 1, nil)

    if left_hand_actor == nil then
        print("Failed to spawn left hand actor")
        return
    end

    right_hand_actor = spawn_actor(world, actor_c, pos, 1, nil)

    if right_hand_actor == nil then
        print("Failed to spawn right hand actor")
        return
    end

    print("Spawned hand actors")

    -- Add scene components to the hand actors
    left_hand_component = left_hand_actor:AddComponentByClass(scene_component_c, false, temp_transform, false)
    right_hand_component = right_hand_actor:AddComponentByClass(scene_component_c, false, temp_transform, false)

    if left_hand_component == nil then
        print("Failed to add left hand scene component")
        return
    end

    if right_hand_component == nil then
        print("Failed to add right hand scene component")
        return
    end

    print("Added scene components")

    left_hand_actor:FinishAddComponent(left_hand_component, false, temp_transform)
    right_hand_actor:FinishAddComponent(right_hand_component, false, temp_transform)

    local leftstate = UEVR_UObjectHook.get_or_add_motion_controller_state(left_hand_component)

    if leftstate then
        leftstate:set_hand(0) -- Left hand
        leftstate:set_permanent(true)
    end

    local rightstate = UEVR_UObjectHook.get_or_add_motion_controller_state(right_hand_component)

    if rightstate then
        rightstate:set_hand(1) -- Right hand
        rightstate:set_permanent(true)
    end
end

local head_fname = kismet_string_library:Conv_StringToName("Face")
local muzzle_fx_fname = kismet_string_library:Conv_StringToName("FX_muzzle")

local vr = uevr.params.vr
local camera_component = nil
local my_pawn = nil
local is_paused = false
local is_in_cutscene = false
local mesh = nil
local head_pos = nil
local pawn_pos = nil
local anim_instance = nil
local is_in_full_body_anim = false
local last_crosshair_widget = nil


local function should_vr_mode()
    anim_instance = nil

    if not vr.is_hmd_active() then
        if last_crosshair_widget then
            last_crosshair_widget:AddToViewport(0)
            last_crosshair_widget = nil
        end

        return false
    end

    camera_component = find_camera_component()
    if not camera_component then
        return false
    end

    is_paused = Statics:IsGamePaused(camera_component)

    if is_paused then
        return false
    end

    my_pawn = api:get_local_pawn(0)

    if not my_pawn then
        return false
    end

    is_in_cutscene = SHCharacterStatics:IsCharacterInCutscene(my_pawn)

    if is_in_cutscene then
        return false
    end

    mesh = my_pawn.Mesh

    if not mesh then
        return false
    end

    local animation = my_pawn.Animation

    if animation then
        anim_instance = animation.AnimInstance
    end

    if my_pawn.bHidden == true then return false end

    if is_save_menu_open() then
        return false
    end

    return true
end

local is_allowing_vr_mode = false
local last_roomscale_value = false
local forward = nil
local last_delta = 1.0

uevr.sdk.callbacks.on_pre_engine_tick(function(engine, delta)
    last_delta = delta
    reset_temp_actor_if_deleted()
    reset_hand_actors_if_deleted()

    if left_hand_actor == nil or right_hand_actor == nil then
        spawn_hand_actors()
    end

    if not vr.is_hmd_active() then
        reset_temp_actor()
        --reset_hand_actors()
        return
    end

    local pawn = api:get_local_pawn(0)

    if not pawn then
        reset_temp_actor()
        return
    end

    local crosshair_widgets = SHCrosshairWidget_c:get_objects_matching(false)

    -- Find first widget that has a valid OwnerCharacter
    local crosshair_widget = nil
    for _, widget in ipairs(crosshair_widgets) do
        if widget.OwnerCharacter ~= nil then
            crosshair_widget = widget
            break
        end
    end

    if crosshair_widget == nil then
        reset_temp_actor()
        return
    end

    if temp_actor == nil or last_crosshair_widget ~= crosshair_widget then
        setup_crosshair_actor(pawn, crosshair_widget)
    end

    last_crosshair_widget = crosshair_widget
end)

uevr.sdk.callbacks.on_early_calculate_stereo_view_offset(function(device, view_index, world_to_meters, position, rotation, is_double)
    is_allowing_vr_mode = should_vr_mode()

    if not is_allowing_vr_mode then
        UEVR_UObjectHook.set_disabled(true)

        if last_roomscale_value == true then
            vr.set_mod_value("VR_RoomscaleMovement", "false")
            last_roomscale_value = false
        end

        return
    end

    if not mesh then
        return
    end

    UEVR_UObjectHook.set_disabled(false)

    if last_roomscale_value == false then
        vr.set_mod_value("VR_RoomscaleMovement", "true")
        vr.set_mod_value("VR_DecoupledPitch", "true")
        last_roomscale_value = true
    end
    
    local head_rot = mesh:GetSocketRotation(head_fname)
    forward = kismet_math_library:Conv_RotatorToVector(head_rot)
    forward = kismet_math_library:RotateAngleAxis(forward, 90, Vector3d.new(0, 0, 1))

    head_pos = mesh:GetSocketLocation(head_fname)
    pawn_pos = my_pawn:K2_GetActorLocation()
    position.x = head_pos.X + (forward.X * 10)
    position.y = head_pos.Y + (forward.Y * 10)
    position.z = head_pos.Z + (forward.Z * 10)
    --[[position.x = pawn_pos.X
    position.y = pawn_pos.Y
    position.z = head_pos.Z]]

    if view_index == 1 then
        local game_engine = UEVR_UObjectHook.get_first_object_by_class(game_engine_class)

        local viewport = game_engine.GameViewport
        if viewport == nil then
            print("Viewport is nil")
            return
        end
    
        local world = viewport.World
        if world == nil then
            print("World is nil")
            return
        end

        last_rot = Vector3d.new(rotation.x, rotation.y, rotation.z)

        -- This is where we attach the weapons to the motion controllers
        if anim_instance then
            local equipped_weapon = anim_instance:GetEquippedWeapon()

            if equipped_weapon then
                local weapon_name = equipped_weapon:get_fname():to_string()

                -- Crosshair widget
                if temp_actor then
                    local ignore_actors = {my_pawn, equipped_weapon, temp_actor}

                    local root = equipped_weapon.RootComponent
                    --local mesh = equipped_weapon.Mesh
                    --local NS_MuzzleSmoke = equipped_weapon.NS_MuzzleSmoke
                    local fire_point = equipped_weapon.FirePoint
                    local weapon_pos = fire_point:K2_GetComponentLocation()
                    local forward_vector = root:GetForwardVector()
                    --local muzzle_rot = root:GetSocketRotation(muzzle_fx_fname)
                    --local forward_vector = kismet_math_library:Conv_RotatorToVector(muzzle_rot)
                    --local forward_vector = NS_MuzzleSmoke:GetForwardVector()

                    if weapon_name:find("Shotgun") or weapon_name:find("Rifle") then
                        -- Rotate forward vector by 90 deg on Z axis
                        forward_vector = kismet_math_library:RotateAngleAxis(forward_vector, 90, root:GetUpVector())
                    end

                    local end_pos = weapon_pos + (forward_vector * 8192.0)
                    local hit = kismet_system_library:LineTraceSingle(world, weapon_pos, end_pos, 15, true, ignore_actors, 0, reusable_hit_result, true, zero_color, zero_color, 1.0)
                    local hit_location = nil
                    --temp_actor:K2_SetActorLocation(weapon_pos + (forward_vector * 100.0), false, empty_hitresult, false)
                    if hit then
                        --local hit_result_location = reusable_hit_result.Location
                        --local hit_result_normal = hit_result.ImpactNormal
                        --hit_location = Vector3d.new(hit_result_location.X, hit_result_location.Y, hit_result_location.Z)
                        --temp_actor:K2_SetActorLocation(hit_location, false, empty_hitresult, false)

                        hit_location = weapon_pos + (forward_vector * (reusable_hit_result.Distance * 0.9))
                    else
                        hit_location = weapon_pos + (forward_vector * 8192.0)
                        --temp_actor:K2_SetActorLocation(hit_location, false, empty_hitresult, false)
                    end

                    local delta = weapon_pos - hit_location
                    local len = math.max(0.1, delta:length() * 0.001)
                    
                    temp_actor:SetActorScale3D(Vector3d.new(len, len, len))

                    local rot = kismet_math_library:Conv_VectorToRotator(forward_vector)
                    rot.Yaw = rot.Yaw + 180
                    rot.Roll = 0
                    rot.Pitch = -rot.Pitch
                    --temp_actor:K2_SetActorRotation(rot, false, empty_hitresult, false)
                    temp_actor:K2_SetActorLocationAndRotation(hit_location, rot, false, empty_hitresult, false)
                end

                -- Attach the weapon to the right hand
                local root_component = equipped_weapon.RootComponent

                if root_component then
                    local state = UEVR_UObjectHook.get_or_add_motion_controller_state(root_component)

                    if state then
                        state:set_hand(1) -- Right hand
                        state:set_permanent(true)

                        --print("Equipped weapon: " .. equipped_weapon:get_fname():to_string())
                        local name = equipped_weapon:get_fname():to_string()

                        if name:find("Pistol") then
                            state:set_rotation_offset(Vector3f.new(0.149, 0.084, 0.077))
                            state:set_location_offset(Vector3f.new(-2.982, -10.160, 4.038))
                        elseif name:find("Shotgun") then
                            state:set_rotation_offset(Vector3f.new(0.037, 1.556, -0.022)) -- Euler (radians)
                            state:set_location_offset(Vector3f.new(-7.967, -5.752, -0.255))
                        elseif name:find("Rifle") then
                            state:set_rotation_offset(Vector3f.new(0.037, 1.556, -0.022)) -- Euler (radians)
                            state:set_location_offset(Vector3f.new(-30.942, -6.758, 0.017))
                        end
                    end
                end
            end
        end
    end
end)


uevr.sdk.callbacks.on_post_calculate_stereo_view_offset(function(device, view_index, world_to_meters, position, rotation, is_double)
    if not camera_component then
        return
    end

    if not is_allowing_vr_mode then
        return
    end

    if view_index ~= 1 then
        return
    end

    -- Using the map and some other things
    if anim_instance and anim_instance.bWholeBodyAnimation then
        return
    end

    local camera_delta_to_head = Vector3d.new(position.x - head_pos.X, position.y - head_pos.Y, position.z - head_pos.Z) - (forward * 10)
    camera_delta_to_head.z = 0
    local mesh_pos = mesh:K2_GetComponentLocation()
    --mesh:K2_SetWorldLocation(Vector3d.new(mesh_pos.X + camera_delta_to_head.x, mesh_pos.Y + camera_delta_to_head.y, mesh_pos.Z), false, empty_hitresult, false)
    --mesh:K2_AddWorldOffset(camera_delta_to_head, false, empty_hitresult, false)


    local rotdelta = rotation - last_rot

    -- Fix up the rotation delta
    if rotdelta.x > 180 then
        rotdelta.x = rotdelta.x - 360
    elseif rotdelta.x < -180 then
        rotdelta.x = rotdelta.x + 360
    end

    if rotdelta.y > 180 then
        rotdelta.y = rotdelta.y - 360
    elseif rotdelta.y < -180 then
        rotdelta.y = rotdelta.y + 360
    end

    if rotdelta.z > 180 then
        rotdelta.z = rotdelta.z - 360
    elseif rotdelta.z < -180 then
        rotdelta.z = rotdelta.z + 360
    end

    local attach_parent = camera_component.AttachParent -- Spring arm
    if attach_parent then
        attach_parent.bEnableCameraRotationLag = false
        attach_parent.bEnableCameraLag = false

        -- View component
        local attach_parent_parent = attach_parent.AttachParent

        if attach_parent_parent then -- bHidden gets set when looking at some stuff
            -- this is how we make the internal camera aim towards the HMD rotation
            attach_parent_parent:AddToControlRotation(rotdelta, nil)
        end
    end

    vr.recenter_view()

    local ik = SHAnimIKHandIKSubcomp_c:get_first_object_matching(false)

    if ik then
        if anim_instance then
            --ik:SetRightHandLocation(equipped_weapon.RootComponent:K2_GetComponentLocation(), 1.0, false)
            ik:SetRightHandLocation(right_hand_component:K2_GetComponentLocation(), 1.0, true)
            ik:SetLeftHandLocation(left_hand_component:K2_GetComponentLocation(), 1.0, true)
        end
    end
end)

uevr.sdk.callbacks.on_script_reset(function()
    print("Resetting")

    reset_temp_actor()
    reset_hand_actors()
end)