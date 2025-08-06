#include "scene_synchronizer.h"

using namespace godot;

Object *SceneSynchronizer::_get_prop_target(Object *p_obj, const NodePath &p_path) {
	if (p_path.get_name_count() == 0) {
		return p_obj;
	}
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V_MSG(!node || !node->has_node(p_path), nullptr, vformat("Node '%s' not found.", p_path));
	return node->get_node<Object>(p_path);
}

Vector<StringName> SceneSynchronizer::_get_subnames(const NodePath &p_path) {
	Vector<StringName> subnames;
	int64_t count = p_path.get_subname_count();
	subnames.resize(count);

	for (int64_t i = 0; i < count; i++) {
		subnames.set(i, p_path.get_subname(i));
	}

	return subnames;
}

void SceneSynchronizer::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	reset();
}

void SceneSynchronizer::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	reset();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		root_node_cache = node->get_instance_id();
		_update_process();
	}
}

void SceneSynchronizer::_update_process() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (!node) {
		return;
	}
	set_process_internal(false);
	set_physics_process_internal(false);
}

Node *SceneSynchronizer::get_root_node() {
	return root_node_cache.is_valid() ? cast_to<Node>(ObjectDB::get_instance(root_node_cache)) : nullptr;
}

void SceneSynchronizer::reset() {
	net_id = 0;
	last_sync_usec = 0;
	last_inbound_sync = 0;
	last_watch_usec = 0;
	sync_started = false;
	watchers.clear();
}

uint32_t SceneSynchronizer::get_net_id() const {
	return net_id;
}

void SceneSynchronizer::set_net_id(uint32_t p_net_id) {
	net_id = p_net_id;
}

bool SceneSynchronizer::update_outbound_sync_time(uint64_t p_usec) {
	if (last_sync_usec == p_usec) {
		// last_sync_usec has been updated in this frame.
		return true;
	}
	if (p_usec < last_sync_usec + sync_interval_usec) {
		// Too soon, should skip this synchronization frame.
		return false;
	}
	last_sync_usec = p_usec;
	return true;
}

bool SceneSynchronizer::update_inbound_sync_time(uint16_t p_network_time) {
	if (!sync_started) {
		sync_started = true;
	} else if (p_network_time <= last_inbound_sync && last_inbound_sync - p_network_time < 32767) {
		return false;
	}
	last_inbound_sync = p_network_time;
	return true;
}

PackedStringArray SceneSynchronizer::_get_configuration_warnings() const {
	PackedStringArray warnings;

	if (root_path.is_empty() || !has_node(root_path)) {
		warnings.push_back("A valid NodePath must be set in the \"Root Path\" property in order for SceneSynchronizer to be able to synchronize properties.");
	}

	if (replication_config.is_null()) {
		warnings.push_back("A valid MultiplayerSynchronizer must be set in the \"Multiplayer Synchronizer Path\" property in order for SceneSynchronizer to be able to synchronize properties.");
	}

	return warnings;
}

void SceneSynchronizer::_set_indexed(Object *p_obj, const Vector<StringName> &p_names, const Variant &p_value, bool *r_valid) {
	if (p_names.is_empty()) {
		if (r_valid) {
			*r_valid = false;
		}
		return;
	}
	if (p_names.size() == 1) {
		p_obj->set(p_names[0], p_value);
		return;
	}

	bool valid = false;
	if (!r_valid) {
		r_valid = &valid;
	}

	List<Variant> value_stack;
	Variant current_value = p_obj->get(p_names[0]);

	if (current_value.get_type() == Variant::NIL) {
		value_stack.clear();
		return;
	}

	value_stack.push_back(current_value);

	for (int i = 1; i < p_names.size() - 1; i++) {
		value_stack.push_back(value_stack.back()->get().get_named(p_names[i], valid));
		*r_valid = valid;

		if (!valid) {
			value_stack.clear();
			return;
		}
	}

	value_stack.push_back(p_value); // p_names[p_names.size() - 1]

	for (int i = p_names.size() - 1; i > 0; i--) {
		value_stack.back()->prev()->get().set_named(p_names[i], value_stack.back()->get(), valid);
		value_stack.pop_back();

		*r_valid = valid;
		if (!valid) {
			value_stack.clear();
			return;
		}
	}

	p_obj->set(p_names[0], value_stack.back()->get());
	value_stack.pop_back();

	ERR_FAIL_COND(!value_stack.is_empty());
}

Variant SceneSynchronizer::_get_indexed(const Object *p_obj, const Vector<StringName> &p_names, bool *r_valid) {
	if (p_names.is_empty()) {
		if (r_valid) {
			*r_valid = false;
		}
		return Variant();
	}
	bool valid = false;

	Variant current_value = p_obj->get(p_names[0]);
	for (int i = 1; i < p_names.size(); i++) {
		current_value = current_value.get_named(p_names[i], valid);

		if (current_value.get_type() == Variant::NIL) {
			break;
		}
	}
	if (r_valid) {
		*r_valid = valid;
	}

	return current_value;
}

Variant SceneSynchronizer::get_state(const TypedArray<NodePath> &p_properties, Object *p_obj, Array r_values) {
	ERR_FAIL_NULL_V(p_obj, ERR_INVALID_PARAMETER);
	r_values.resize(p_properties.size());
	int i = 0;
	for (const NodePath prop : p_properties) {
		const Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_NULL_V(obj, FAILED);
		Variant result = _get_indexed(obj, _get_subnames(prop));
		ERR_FAIL_COND_V_MSG(result.get_type() == Variant::NIL, ERR_INVALID_DATA, vformat("Property '%s' not found.", prop));
		r_values[i] = result;
		i++;
	}
	return OK;
}

Variant SceneSynchronizer::set_state(const TypedArray<NodePath> &p_properties, Object *p_obj, const Array &p_state) {
	ERR_FAIL_NULL_V(p_obj, ERR_INVALID_PARAMETER);
	int i = 0;
	for (const NodePath prop : p_properties) {
		Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_NULL_V(obj, FAILED);
		_set_indexed(obj, _get_subnames(prop), p_state[i]);
		i += 1;
	}
	return OK;
}
void SceneSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_multiplayer_synchronizer", "synchronizer"), &SceneSynchronizer::set_multiplayer_synchronizer);
	ClassDB::bind_method(D_METHOD("get_multiplayer_synchronizer"), &SceneSynchronizer::get_multiplayer_synchronizer);

	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &SceneSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &SceneSynchronizer::get_root_path);

	ClassDB::bind_method(D_METHOD("set_replication_interval", "milliseconds"), &SceneSynchronizer::set_replication_interval);
	ClassDB::bind_method(D_METHOD("get_replication_interval"), &SceneSynchronizer::get_replication_interval);

	ClassDB::bind_method(D_METHOD("set_delta_interval", "milliseconds"), &SceneSynchronizer::set_delta_interval);
	ClassDB::bind_method(D_METHOD("get_delta_interval"), &SceneSynchronizer::get_delta_interval);

	ClassDB::bind_method(D_METHOD("get_replication_config"), &SceneSynchronizer::get_replication_config);

	ClassDB::bind_method(D_METHOD("get_net_id"), &SceneSynchronizer::get_net_id);
	ClassDB::bind_method(D_METHOD("set_net_id", "net_id"), &SceneSynchronizer::set_net_id);

	ClassDB::bind_method(D_METHOD("update_outbound_sync_time", "usec"), &SceneSynchronizer::update_outbound_sync_time);
	ClassDB::bind_method(D_METHOD("update_inbound_sync_time", "network_time"), &SceneSynchronizer::update_inbound_sync_time);

	ClassDB::bind_static_method("SceneSynchronizer", D_METHOD("set_state", "properties", "object", "state"), &SceneSynchronizer::set_state);
	ClassDB::bind_static_method("SceneSynchronizer", D_METHOD("get_state", "properties", "object", "state"), &SceneSynchronizer::get_state);

	ClassDB::bind_method(D_METHOD("get_delta_state", "cur_usec", "last_usec", "delta_props", "delta_values"), &SceneSynchronizer::get_delta_state);
	ClassDB::bind_method(D_METHOD("get_delta_state_encoded", "cur_usec", "last_usec", "delta_props", "delta_values"), &SceneSynchronizer::get_delta_state_encoded);

	ClassDB::bind_method(D_METHOD("get_sync_state", "cur_usec", "last_usec", "sync_props", "sync_values"), &SceneSynchronizer::get_sync_state);
	ClassDB::bind_method(D_METHOD("get_sync_state_encoded", "cur_usec", "last_usec", "sync_props", "sync_values"), &SceneSynchronizer::get_sync_state_encoded);

	ClassDB::bind_method(D_METHOD("get_delta_properties"), &SceneSynchronizer::get_delta_properties);
	ClassDB::bind_method(D_METHOD("get_watch_properties"), &SceneSynchronizer::get_delta_properties);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiplayer_synchronizer", PROPERTY_HINT_NODE_TYPE, "MultiplayerSynchronizer"), "set_multiplayer_synchronizer", "get_multiplayer_synchronizer");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "replication_interval", PROPERTY_HINT_RANGE, "0,5,0.001,suffix:s"), "set_replication_interval", "get_replication_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "delta_interval", PROPERTY_HINT_RANGE, "0,5,0.001,suffix:s"), "set_delta_interval", "get_delta_interval");
}

void SceneSynchronizer::set_replication_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Interval must be greater or equal to 0 (where 0 means default)");
	sync_interval_usec = uint64_t(p_interval * 1000 * 1000);
}

double SceneSynchronizer::get_replication_interval() const {
	return double(sync_interval_usec) / 1000.0 / 1000.0;
}

void SceneSynchronizer::set_delta_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Interval must be greater or equal to 0 (where 0 means default)");
	delta_interval_usec = uint64_t(p_interval * 1000 * 1000);
}

double SceneSynchronizer::get_delta_interval() const {
	return double(delta_interval_usec) / 1000.0 / 1000.0;
}

Ref<SceneReplicationConfig> SceneSynchronizer::get_replication_config() {
	return replication_config;
}

void SceneSynchronizer::set_multiplayer_synchronizer(MultiplayerSynchronizer *p_synchronizer) {
	multiplayer_synchronizer = p_synchronizer;
	replication_config = multiplayer_synchronizer ? multiplayer_synchronizer->get_replication_config() : Ref<SceneReplicationConfig>();
	update_configuration_warnings();
}

MultiplayerSynchronizer *SceneSynchronizer::get_multiplayer_synchronizer() const {
	return multiplayer_synchronizer;
}

void SceneSynchronizer::set_root_path(const NodePath &p_path) {
	if (p_path == root_path) {
		return;
	}
	_stop();
	root_path = p_path;
	_start();
	update_configuration_warnings();
}

NodePath SceneSynchronizer::get_root_path() const {
	return root_path;
}

// TODO: Caching? Godot internal has access to these without copying. Unfortunately, they're not exposed. Even if they
//       were, the API would almost certainly return a copy.
TypedArray<NodePath> SceneSynchronizer::_get_sync_properties() {
	TypedArray<NodePath> sync_props;
	if (replication_config.is_null()) {
		return sync_props;
	}

	TypedArray<NodePath> all_props = replication_config->get_properties();
	sync_props.resize(all_props.size());
	int count = 0;

	for (NodePath prop : replication_config->get_properties()) {
		if (replication_config->property_get_sync(prop)) {
			sync_props.push_back(prop);
			count++;
		}
	}

	sync_props.resize(count);
	return sync_props;
}

// TODO: Caching? Godot internal has access to these without copying. Unfortunately, they're not exposed. Even if they
//       were, the API would almost certainly return a copy.
TypedArray<NodePath> SceneSynchronizer::_get_watch_properties() {
	TypedArray<NodePath> watch_props;
	if (replication_config.is_null()) {
		return watch_props;
	}

	TypedArray<NodePath> all_props = replication_config->get_properties();
	watch_props.resize(all_props.size());
	int count = 0;

	for (NodePath prop : replication_config->get_properties()) {
		if (replication_config->property_get_watch(prop)) {
			watch_props.push_back(prop);
			count++;
		}
	}

	watch_props.resize(count);
	return watch_props;
}

Error SceneSynchronizer::_watch_changes(uint64_t p_usec) {
	ERR_FAIL_COND_V(replication_config.is_null(), FAILED);
	const TypedArray<NodePath> props = _get_watch_properties();

	if (props.size() != watchers.size()) {
		watchers.resize(props.size());
	}
	if (props.is_empty()) {
		return OK;
	}
	Node *node = get_root_node();
	ERR_FAIL_NULL_V(node, FAILED);
	int idx = -1;
	Watcher *ptr = watchers.ptrw();
	for (const NodePath &prop : props) {
		idx++;
		const Object *obj = _get_prop_target(node, prop);
		ERR_CONTINUE_MSG(!obj, vformat("Node not found for property '%s'.", prop));
		Variant v = _get_indexed(obj, _get_subnames(prop));
		ERR_CONTINUE_MSG(v.get_type() == Variant::NIL, vformat("Property '%s' not found.", prop));
		Watcher &w = ptr[idx];
		if (w.prop != prop) {
			w.prop = prop;
			w.value = v.duplicate(true);
			w.last_change_usec = p_usec;
		} else if (!w.value.hash_compare(v)) {
			w.value = v.duplicate(true);
			w.last_change_usec = p_usec;
		}
	}
	return OK;
}

void SceneSynchronizer::get_sync_state(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_sync_props, Array r_sync_values) {
	if (p_cur_usec < p_last_usec + sync_interval_usec) {
		// Too soon skip sync synchronization.
		return;
	}

	Node *node = get_root_node();
	r_sync_props = _get_sync_properties();

	SceneSynchronizer::get_state(r_sync_props, node, r_sync_values);
}

void SceneSynchronizer::get_sync_state_encoded(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_sync_props, TypedArray<PackedByteArray> r_sync_values_encoded) {
	TypedArray<NodePath> sync_values;
	SceneSynchronizer::get_sync_state(p_cur_usec, p_last_usec, r_sync_props, sync_values);

	r_sync_values_encoded.resize(sync_values.size());
	for (int i = 0; i < sync_values.size(); i++) {
		r_sync_values_encoded[i] = UtilityFunctions::var_to_bytes(sync_values[i]);
	}
}

void SceneSynchronizer::get_delta_state(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_delta_props, Array r_delta_values) {
	if (last_watch_usec == p_cur_usec) {
		// We already watched for changes in this frame.

	} else if (p_cur_usec < p_last_usec + delta_interval_usec) {
		// Too soon skip delta synchronization.
		return;

	} else {
		// Watch for changes.
		Error err = _watch_changes(p_cur_usec);
		ERR_FAIL_COND(err != OK);
		last_watch_usec = p_cur_usec;
	}

	auto size = watchers.size();
	r_delta_props.resize(size);
	r_delta_values.resize(size);

	const Watcher *ptr = size ? watchers.ptr() : nullptr;

	for (int i = 0; i < size; i++) {
		const Watcher &w = ptr[i];
		if (w.last_change_usec <= p_last_usec) {
			continue;
		}
		r_delta_props[i] = w.prop;
		r_delta_values[i] = w.value;
	}
}

void SceneSynchronizer::get_delta_state_encoded(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_delta_props, TypedArray<PackedByteArray> r_delta_values_encoded) {
	TypedArray<NodePath> delta_values;
	SceneSynchronizer::get_delta_state(p_cur_usec, p_last_usec, r_delta_props, delta_values);

	r_delta_values_encoded.resize(delta_values.size());
	for (int i = 0; i < delta_values.size(); i++) {
		r_delta_values_encoded[i] = UtilityFunctions::var_to_bytes(delta_values[i]);
	}
}

TypedArray<NodePath> SceneSynchronizer::get_delta_properties() {
	ERR_FAIL_COND_V(replication_config.is_null(), TypedArray<NodePath>());
	return _get_watch_properties();
}

TypedArray<NodePath> SceneSynchronizer::get_sync_properties() {
	ERR_FAIL_COND_V(replication_config.is_null(), TypedArray<NodePath>());
	return _get_sync_properties();
}

SceneReplicationConfig *SceneSynchronizer::get_replication_config_ptr() const {
	return replication_config.ptr();
}

SceneSynchronizer::SceneSynchronizer() {
}

void SceneSynchronizer::_enter_tree() {
	Node::_enter_tree();

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	if (root_path.is_empty()) {
		return;
	}

	_start();
}

void SceneSynchronizer::_exit_tree() {
	Node::_exit_tree();
	_stop();
}
