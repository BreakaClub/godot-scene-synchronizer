#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "node_serializer.h"
#include "scene_synchronizer.h"

using namespace godot;

void initialize_scene_synchronizer_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	NodeSerializer::initialize();

	GDREGISTER_CLASS(NodeSerializer);
	GDREGISTER_CLASS(SceneSynchronizer);
}

void uninitialize_scene_synchronizer_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	NodeSerializer::cleanup();
}

extern "C" {

// Initialization.

GDExtensionBool GDE_EXPORT scene_synchronizer_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_scene_synchronizer_module);
	init_obj.register_terminator(uninitialize_scene_synchronizer_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
