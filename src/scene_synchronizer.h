#pragma once

#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/multiplayer_synchronizer.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_replication_config.hpp>
#include <godot_cpp/classes/wrapped.hpp>

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

class SceneSynchronizer : public Node {
	GDCLASS(SceneSynchronizer, Node);

private:
	struct Watcher {
		NodePath prop;
		uint64_t last_change_usec = 0;
		Variant value;
	};

	Ref<SceneReplicationConfig> replication_config;
	NodePath root_path = NodePath(".."); // Start with parent, like with AnimationPlayer.
	MultiplayerSynchronizer *multiplayer_synchronizer = nullptr;
	uint64_t sync_interval_usec = 0;
	uint64_t delta_interval_usec = 0;
	Vector<Watcher> watchers;
	uint64_t last_watch_usec = 0;

	ObjectID root_node_cache;
	uint64_t last_sync_usec = 0;
	uint16_t last_inbound_sync = 0;
	uint32_t net_id = 0;
	bool sync_started = false;

	static Object *_get_prop_target(Object *p_obj, const NodePath &p_prop);
	static Vector<StringName> _get_subnames(const NodePath &p_path);
	static void _set_indexed(Object *p_obj, const Vector<StringName> &p_names, const Variant &p_value, bool *r_valid = nullptr);
	static Variant _get_indexed(const Object *p_obj, const Vector<StringName> &p_names, bool *r_valid = nullptr);
	void _start();
	void _stop();
	void _update_process();
	Error _watch_changes(uint64_t p_usec);
	TypedArray<NodePath> _get_sync_properties();
	TypedArray<NodePath> _get_watch_properties();

protected:
	static void _bind_methods();

public:
	static Variant get_state(const TypedArray<NodePath> &p_properties, Object *p_obj, Array r_values);
	static Variant set_state(const TypedArray<NodePath> &p_properties, Object *p_obj, const Array &p_state);

	void _enter_tree() override;
	void _exit_tree() override;
	PackedStringArray _get_configuration_warnings() const override;

	void reset();
	Node *get_root_node();

	uint32_t get_net_id() const;
	void set_net_id(uint32_t p_net_id);

	bool update_outbound_sync_time(uint64_t p_usec);
	bool update_inbound_sync_time(uint16_t p_network_time);

	void set_replication_interval(double p_interval);
	double get_replication_interval() const;

	void set_delta_interval(double p_interval);
	double get_delta_interval() const;

	Ref<SceneReplicationConfig> get_replication_config();

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	void set_multiplayer_synchronizer(MultiplayerSynchronizer *p_synchronizer);
	MultiplayerSynchronizer *get_multiplayer_synchronizer() const;

	void get_sync_state(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_sync_props, Array r_sync_values);
	void get_sync_state_encoded(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_sync_props, TypedArray<PackedByteArray> r_sync_values_encoded);

	void get_delta_state(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_delta_props, Array r_delta_values);
	void get_delta_state_encoded(uint64_t p_cur_usec, uint64_t p_last_usec, TypedArray<NodePath> r_delta_props, TypedArray<PackedByteArray> r_delta_values_encoded);

	TypedArray<NodePath> get_delta_properties();
	TypedArray<NodePath> get_sync_properties();

	SceneReplicationConfig *get_replication_config_ptr() const;

	SceneSynchronizer();
};

} //namespace godot
