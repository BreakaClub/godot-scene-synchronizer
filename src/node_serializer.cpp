#include "node_serializer.h"
#include "instrumentation.h"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

HashMap<String, NodeSerializer::ObjectRegistration *> NodeSerializer::_script_registry;

StringName *NodeSerializer::FIELD_CHILDREN = nullptr;
StringName *NodeSerializer::FIELD_SCENE = nullptr;
StringName *NodeSerializer::FIELD_TYPE = nullptr;
StringName *NodeSerializer::TYPE_UNSERIALIZABLE_CHILD = nullptr;
StringName *NodeSerializer::TYPE_NAME_NATIVE = nullptr;
StringName *NodeSerializer::TYPE_NAME_PACKED_BYTE_ARRAY = nullptr;
StringName *NodeSerializer::FIELD_DATA = nullptr;
StringName *NodeSerializer::TYPE_NAME_TYPED_GDICTIONARY = nullptr;
StringName *NodeSerializer::FIELD_ENTRIES = nullptr;
StringName *NodeSerializer::FIELD_KEY_TYPE = nullptr;
StringName *NodeSerializer::FIELD_KEY_CLASS_NAME = nullptr;
StringName *NodeSerializer::FIELD_VALUE_TYPE = nullptr;
StringName *NodeSerializer::FIELD_VALUE_CLASS_NAME = nullptr;
StringName *NodeSerializer::PROPERTY_SCRIPT = nullptr;
StringName *NodeSerializer::PROPERTY_RESOURCE_PATH = nullptr;
StringName *NodeSerializer::METHOD_SERIALIZE = nullptr;

void NodeSerializer::initialize() {
	FIELD_CHILDREN = new StringName("._children");
	FIELD_SCENE = new StringName("._scn");
	FIELD_TYPE = new StringName("._type");
	TYPE_UNSERIALIZABLE_CHILD = new StringName("._");
	TYPE_NAME_NATIVE = new StringName("._n");
	TYPE_NAME_PACKED_BYTE_ARRAY = new StringName("._b64");
	FIELD_DATA = new StringName("data");
	TYPE_NAME_TYPED_GDICTIONARY = new StringName("._ty_d");
	FIELD_ENTRIES = new StringName("entries");
	FIELD_KEY_TYPE = new StringName("keyType");
	FIELD_KEY_CLASS_NAME = new StringName("keyClassName");
	FIELD_VALUE_TYPE = new StringName("valueType");
	FIELD_VALUE_CLASS_NAME = new StringName("valueClassName");
	PROPERTY_SCRIPT = new StringName("script");
	PROPERTY_RESOURCE_PATH = new StringName("resource_path");
	METHOD_SERIALIZE = new StringName("_serialize");
}

void NodeSerializer::cleanup() {
	delete FIELD_CHILDREN;
	delete FIELD_SCENE;
	delete FIELD_TYPE;
	delete TYPE_UNSERIALIZABLE_CHILD;
	delete TYPE_NAME_NATIVE;
	delete TYPE_NAME_PACKED_BYTE_ARRAY;
	delete FIELD_DATA;
	delete TYPE_NAME_TYPED_GDICTIONARY;
	delete FIELD_ENTRIES;
	delete FIELD_KEY_TYPE;
	delete FIELD_KEY_CLASS_NAME;
	delete FIELD_VALUE_TYPE;
	delete FIELD_VALUE_CLASS_NAME;
	delete PROPERTY_SCRIPT;
	delete PROPERTY_RESOURCE_PATH;
	delete METHOD_SERIALIZE;

	for (const KeyValue<String, ObjectRegistration *> &E : _script_registry) {
		memdelete(E.value);
	}
	_script_registry.clear();
}

void NodeSerializer::_bind_methods() {
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("register_serializable_class", "script_or_path", "mutable_property_list"), &NodeSerializer::register_serializable_class, DEFVAL(false));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("serialize_to_json_structure", "value", "options"), &NodeSerializer::serialize_to_json_structure, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("serialize_to_json", "value", "indent", "sort_keys", "full_precision", "options"), &NodeSerializer::serialize_to_json, DEFVAL(""), DEFVAL(false), DEFVAL(false), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("deserialize_from_json_structure", "json_string", "options"), &NodeSerializer::deserialize_from_json_structure, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("deserialize_from_json", "json_string", "options"), &NodeSerializer::deserialize_from_json, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("serialize_to_binary_structure", "value", "options"), &NodeSerializer::serialize_to_binary_structure, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("serialize_to_binary", "value", "options"), &NodeSerializer::serialize_to_binary, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("deserialize_from_binary_structure", "value", "options"), &NodeSerializer::deserialize_from_binary_structure, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("NodeSerializer", D_METHOD("deserialize_from_binary", "bytes", "options"), &NodeSerializer::deserialize_from_binary, DEFVAL(Dictionary()));
}

void NodeSerializer::register_serializable_class(const Variant &p_script_or_path, bool p_mutable_property_list) {
	Ref<Script> script;
	String path;

	if (p_script_or_path.get_type() == Variant::STRING) {
		path = p_script_or_path;
	} else {
		script = p_script_or_path;

		if (script.is_valid()) {
			path = script->get_path();
		}
	}

	ERR_FAIL_COND_MSG(path.is_empty(), "Serializable class name/path cannot be empty.");

	if (!script.is_valid() && path.begins_with("res://")) {
		script = ResourceLoader::get_singleton()->load(path);
	}

	ERR_FAIL_COND_V_MSG(!script.is_valid(), void(), "Failed to load script for serialization: " + path);

	if (_script_registry.has(path)) {
		WARN_PRINT("Overwriting existing serializable class registration for: " + path);
		memdelete(_script_registry[path]);
	}

	ObjectRegistration *reg = memnew(ObjectRegistration);
	reg->name = path;
	reg->script = script;
	reg->mutable_property_list = p_mutable_property_list;
	_script_registry[path] = reg;
}

Variant NodeSerializer::serialize_to_json_structure(const Variant &p_value, const Dictionary &p_options) {
	INSTRUMENT_FUNCTION_START("serialize_to_json_structure");
	SerializationContext context;
	context.serialize_value = &_json_serialize_value;
	_apply_serialization_context_options(context, p_options);
	Variant result = _json_serialize_value(p_value, context);
	INSTRUMENT_FUNCTION_END();
	return result;
}

String NodeSerializer::serialize_to_json(const Variant &p_value, const String &p_indent, bool p_sort_keys, bool p_full_precision, const Dictionary &p_options) {
	return JSON::stringify(serialize_to_json_structure(p_value, p_options), p_indent, p_sort_keys, p_full_precision);
}

Variant NodeSerializer::deserialize_from_json_structure(const Variant &p_value, const Dictionary &p_options) {
	INSTRUMENT_FUNCTION_START("deserialize_from_json_structure");
	DeserializationContext context;
	context.deserialize_value = &_json_deserialize_value;
	_apply_deserialization_context_options(context, p_options);
	Variant result = context.deserialize_value(p_value, context);
	INSTRUMENT_FUNCTION_END();
	return result;
}

Variant NodeSerializer::deserialize_from_json(const String &p_json_string, const Dictionary &p_options) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_json_string);
	ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Failed to parse JSON string: " + json->get_error_message());
	Variant result = deserialize_from_json_structure(json->get_data(), p_options);
	return result;
}

Variant NodeSerializer::serialize_to_binary_structure(const Variant &p_value, const Dictionary &p_options) {
	INSTRUMENT_FUNCTION_START("serialize_to_binary_structure");
	SerializationContext context;
	context.serialize_value = &_serialize_recursively;
	_apply_serialization_context_options(context, p_options);
	Variant result = _serialize_recursively(p_value, context);
	INSTRUMENT_FUNCTION_END();
	return result;
}

PackedByteArray NodeSerializer::serialize_to_binary(const Variant &p_value, const Dictionary &p_options) {
	return UtilityFunctions::var_to_bytes(serialize_to_binary_structure(p_value, p_options));
}

Variant NodeSerializer::deserialize_from_binary_structure(const Variant &p_value, const Dictionary &p_options) {
	INSTRUMENT_FUNCTION_START("deserialize_from_binary_structure");
	DeserializationContext context;
	context.deserialize_value = &_deserialize_recursively;
	_apply_deserialization_context_options(context, p_options);
	Variant result = context.deserialize_value(p_value, context);
	INSTRUMENT_FUNCTION_END();
	return result;
}

Variant NodeSerializer::deserialize_from_binary(const PackedByteArray &p_bytes, const Dictionary &p_options) {
	return deserialize_from_binary_structure(UtilityFunctions::bytes_to_var(p_bytes), p_options);
}

String NodeSerializer::_get_object_registration_name(Object *p_object) {
	if (!p_object) {
		return "";
	}
	Ref<Script> script = p_object->get_script();
	if (script.is_valid() && !script->get_path().is_empty()) {
		return script->get_path();
	}
	return p_object->get_class();
}

NodeSerializer::ObjectRegistration *NodeSerializer::_get_serializable_registration(const String &p_name) {
	if (_script_registry.has(p_name)) {
		return _script_registry[p_name];
	}
	return nullptr;
}

void NodeSerializer::_apply_serialization_context_options(SerializationContext &p_context, const Dictionary &p_options) {
	if (p_options.has("property_name")) {
		p_context.property_name = p_options["property_name"];
	}

	if (p_options.has("required_property_usage_flags")) {
		p_context.required_property_usage_flags = p_options["required_property_usage_flags"];
	}

	if (p_options.has("scene_root_node")) {
		p_context.scene_root_node = Object::cast_to<Node>(p_options["scene_root_node"]);
	}
}

void NodeSerializer::_apply_deserialization_context_options(DeserializationContext &p_context, const Dictionary &p_options) {
	if (p_options.has("scene_root_node")) {
		p_context.scene_root_node = Object::cast_to<Node>(p_options["scene_root_node"]);
	}
}

Variant NodeSerializer::_serialize_recursively(const Variant &p_value, SerializationContext &p_context) {
	if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		Array new_arr;
		int64_t size = arr.size();
		new_arr.resize(size);
		for (int i = 0; i < size; ++i) {
			new_arr[i] = p_context.serialize_value(arr[i], p_context);
		}
		return new_arr;
	}

	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_value;
		Dictionary new_dict;
		for (const auto &key : dict.keys()) {
			new_dict[key] = p_context.serialize_value(dict[key], p_context);
		}
		return new_dict;
	}

	if (p_value.get_type() == Variant::OBJECT) {
		Object *obj = p_value.get_validated_object();
		if (obj) {
			String reg_name = _get_object_registration_name(obj);
			ObjectRegistration *registration = _get_serializable_registration(reg_name);

			if (registration) {
				return registration->serialize(obj, p_context);
			}

			if (p_context.property_name.is_empty()) {
				ERR_PRINT("Unregistered Object (" + reg_name + ") cannot be serialized. Register it first.");
			} else {
				ERR_PRINT("Unregistered Object (" + reg_name + ") cannot be serialized. Register it first. Property: " + p_context.property_name);
			}
		}

		return Variant();
	}

	return p_value;
}

Variant NodeSerializer::_deserialize_recursively(const Variant &p_value, DeserializationContext &p_context) {
	if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		Array new_arr;
		new_arr.resize(arr.size());
		for (int i = 0; i < arr.size(); ++i) {
			new_arr[i] = p_context.deserialize_value(arr[i], p_context);
		}
		return new_arr;
	}

	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_value;
		if (dict.has(*FIELD_TYPE)) {
			StringName type_name = dict[*FIELD_TYPE];

			if (type_name == *TYPE_NAME_TYPED_GDICTIONARY) {
				int64_t key_type = dict.get(*FIELD_KEY_TYPE, (int64_t)Variant::NIL);
				StringName key_class_name = dict.get(*FIELD_KEY_CLASS_NAME, StringName());
				int64_t value_type = dict.get(*FIELD_VALUE_TYPE, (int64_t)Variant::NIL);
				StringName value_class_name = dict.get(*FIELD_VALUE_CLASS_NAME, StringName());

				ObjectRegistration *key_reg = !key_class_name.is_empty() ? _get_serializable_registration(key_class_name) : nullptr;
				ObjectRegistration *value_reg = !value_class_name.is_empty() ? _get_serializable_registration(value_class_name) : nullptr;

				Ref<Script> key_script = key_reg ? key_reg->script : Ref<Script>();
				Ref<Script> value_script = value_reg ? value_reg->script : Ref<Script>();

				Dictionary new_typed_dict = Dictionary(Dictionary(), key_type, key_class_name, key_script, value_type, value_class_name, value_script);

				Array entries = dict.get(*FIELD_ENTRIES, Array());
				for (Array entry : entries) {
					if (entry.size() == 2) {
						Variant key = p_context.deserialize_value(entry[0], p_context);
						Variant val = p_context.deserialize_value(entry[1], p_context);
						new_typed_dict[key] = val;
					}
				}
				return new_typed_dict;

			} else {
				ObjectRegistration *registration = _get_serializable_registration(type_name);
				if (registration) {
					DeserializationContext inner_context = p_context;
					inner_context.target_object = nullptr;
					return registration->deserialize(dict, inner_context);
				} else {
					ERR_PRINT("Attempted to deserialize unregistered type: " + String(type_name));
					return Variant();
				}
			}
		} else {
			Dictionary new_dict;
			Array keys = dict.keys();
			for (const auto &key : keys) {
				new_dict[key] = p_context.deserialize_value(dict[key], p_context);
			}
			return new_dict;
		}
	}

	return p_value;
}

Variant NodeSerializer::_json_serialize_value(const Variant &p_value, SerializationContext &p_context) {
	Variant serialized_value = _serialize_recursively(p_value, p_context);
	INSTRUMENT_FUNCTION_START_WITH_SERIALIZATION_CONTEXT("_json_serialize_value", serialized_value, p_context);

	switch (serialized_value.get_type()) {
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
		case Variant::NIL:
		case Variant::DICTIONARY:
		case Variant::ARRAY:
			INSTRUMENT_FUNCTION_END();
			return serialized_value;
		case Variant::PACKED_BYTE_ARRAY: {
			Dictionary packed_representation;
			packed_representation[*FIELD_TYPE] = *TYPE_NAME_PACKED_BYTE_ARRAY;
			packed_representation[*FIELD_DATA] = Marshalls::get_singleton()->raw_to_base64(serialized_value);
			INSTRUMENT_FUNCTION_END();
			return packed_representation;
		}
		case Variant::OBJECT: {
			Object *obj = serialized_value.get_validated_object();
			if (obj) {
				ERR_PRINT("Unregistered Object (" + _get_object_registration_name(obj) + +") cannot be serialized. Register it first.");
			} else {
				ERR_PRINT("Unregistered Object cannot be serialized. Register it first.");
			}
			INSTRUMENT_FUNCTION_END();
			return Variant();
		}
		default: {
			Dictionary native_representation;
			native_representation[*FIELD_TYPE] = *TYPE_NAME_NATIVE;
            native_representation[*FIELD_DATA] = Marshalls::get_singleton()->variant_to_base64(serialized_value, false);
//			native_representation[*FIELD_DATA] = JSON::from_native(serialized_value);
			INSTRUMENT_FUNCTION_END();
			return native_representation;
		}
	}
}

Variant NodeSerializer::_json_deserialize_value(const Variant &p_value, DeserializationContext &p_context) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_value;
		if (dict.has(*FIELD_TYPE) && dict.has(*FIELD_DATA)) {
			StringName type_name = dict[*FIELD_TYPE];
			if (type_name == *TYPE_NAME_PACKED_BYTE_ARRAY) {
				return Marshalls::get_singleton()->base64_to_raw(dict[*FIELD_DATA]);
			}
			if (type_name == *TYPE_NAME_NATIVE) {
				Ref<JSON> json;
				json.instantiate();
				json->parse(dict[*FIELD_DATA]);
				return json->get_data();
			}
		}
	}

	return _deserialize_recursively(p_value, p_context);
}

Dictionary NodeSerializer::_serialize_children(Node *p_node, SerializationContext &p_context) {
	Dictionary serialized_children;

	for (int i = 0; i < p_node->get_child_count(); ++i) {
		Node *child = p_node->get_child(i);
		String child_name = child->get_name();

		String registration_name = _get_object_registration_name(child);
		ObjectRegistration *registration = _get_serializable_registration(registration_name);

		Object *previous_target_object = p_context.target_object;
		p_context.target_object = child;

		if (registration) {
			Dictionary serialized_child = registration->serialize(child, p_context);

			if (!serialized_child.is_empty()) {
				serialized_children[child_name] = serialized_child;
			}
		} else {
			Dictionary grandchildren = _serialize_children(child, p_context);

			if (!grandchildren.is_empty()) {
				Dictionary unserializable_child_data;
				unserializable_child_data[*FIELD_TYPE] = *TYPE_UNSERIALIZABLE_CHILD;
				unserializable_child_data[*FIELD_CHILDREN] = grandchildren;
				serialized_children[child_name] = unserializable_child_data;
			}
		}

		p_context.target_object = previous_target_object;
	}
	return serialized_children;
}

void NodeSerializer::_deserialize_children(Node *node, const Dictionary &p_serialized_children, DeserializationContext &p_context) {
	Array child_names = p_serialized_children.keys();
	for (String child_name : child_names) {
		Dictionary child_data = p_serialized_children[child_name];
		StringName child_type = child_data.get(*FIELD_TYPE, StringName());

		Node *child_node = node->get_node_or_null(NodePath(child_name));

		if (child_type == *TYPE_UNSERIALIZABLE_CHILD) {
			if (child_node) {
				_deserialize_children(child_node, child_data.get(*FIELD_CHILDREN, Dictionary()), p_context);
			} else {
				WARN_PRINT("Unable to find expected child during deserialization: " + node->get_path().get_concatenated_subnames() + "/" + child_name);
			}
			continue;
		}

		ObjectRegistration *registration = _get_serializable_registration(child_type);
		if (!registration) {
			ERR_PRINT("Encountered unregistered type: " + String(child_type) + " at: " + node->get_path().get_concatenated_subnames() + "/" + child_name);
			continue;
		}

		DeserializationContext child_context = p_context;
		child_context.target_object = nullptr;

		if (child_node) {
			if (_get_object_registration_name(child_node) == String(child_type)) {
				child_context.target_object = child_node;
			} else {
				node->remove_child(child_node);
				child_node->queue_free();
			}
		}

		Node *new_or_updated_node = Object::cast_to<Node>(registration->deserialize(child_data, child_context));

		if (new_or_updated_node && !new_or_updated_node->get_parent()) {
			node->add_child(new_or_updated_node);
			new_or_updated_node->set_name(child_name);
		} else if (new_or_updated_node && new_or_updated_node->get_name() != child_name) {
			new_or_updated_node->set_name(child_name);
		}
	}
}

Dictionary NodeSerializer::ObjectRegistration::serialize(Object *p_object, SerializationContext &p_context) const {
	p_context.target_object = p_object;
	StringName previous_property_name = p_context.property_name;
	p_context.property_name = StringName();

	Node *node = Object::cast_to<Node>(p_object);

	if (node) {
		String scene_path = node->get_scene_file_path();

		if (!scene_path.is_empty()) {
			p_context.scene_root_node = node;
		}
	}

	if (unlikely(has_custom_serializer != HasMethod::No)) {
		if (unlikely(has_custom_serializer == HasMethod::Unchecked)) {
			has_custom_serializer = p_object->has_method(*METHOD_SERIALIZE) ? HasMethod::Yes : HasMethod::No;
		}

		if (has_custom_serializer == HasMethod::Yes) {
			Variant serialized_value = p_object->call(*METHOD_SERIALIZE);

			if (serialized_value.get_type() == Variant::DICTIONARY) {
				Dictionary serialized_dict = serialized_value;

				if (!serialized_dict.has(*FIELD_TYPE)) {
					serialized_dict[*FIELD_TYPE] = this->name;
				}

				p_context.property_name = previous_property_name;
				return serialized_dict;
			}

			p_context.property_name = previous_property_name;
			return Dictionary();
		}
	}

	p_context.property_name = previous_property_name;
	return _default_serialize(p_object, p_context);
}

const std::map<StringName, NodeSerializer::PropertyCacheData> &NodeSerializer::ObjectRegistration::_get_property_map(Object *p_object) const {
	if (!mutable_property_list && _cached_property_map.size() > 0) {
		return _cached_property_map;
	}

	_cached_property_map.clear();
	StringName class_name = p_object->get_class();
	Array property_list = p_object->get_property_list();

	for (int64_t i = 0, count = property_list.size(); i < count; ++i) {
		Dictionary property_info = property_list[i];
		StringName property_name = property_info["name"];

		if (property_name == *PROPERTY_SCRIPT || property_name == *PROPERTY_RESOURCE_PATH) {
			continue;
		}

		StringName custom_serializer_method = "_serialize_property_" + property_name;
		bool has_custom_serializer = p_object->has_method(custom_serializer_method);

		StringName custom_deserializer_method = "_deserialize_property_" + property_name;
		bool has_custom_deserializer = p_object->has_method(custom_deserializer_method);

		_cached_property_map[property_name] = {
			.type = Variant::Type(int(property_info["type"])),
			.name = property_name,
			.usage = property_info["usage"],
			.default_value = ClassDB::class_get_property_default_value(class_name, property_name),
			.custom_serializer_name = has_custom_serializer ? custom_serializer_method : StringName(),
			.has_custom_serializer = has_custom_serializer,
			.custom_deserializer_name = has_custom_deserializer ? custom_deserializer_method : StringName(),
			.has_custom_deserializer = has_custom_deserializer,
		};
	}
	return _cached_property_map;
}

Object *NodeSerializer::ObjectRegistration::deserialize(const Dictionary &p_serialized, DeserializationContext &p_context) const {
	Object *object = p_context.target_object;

	if (!object) {
		String scene_path = p_serialized.get(*FIELD_SCENE, "");
		if (!scene_path.is_empty()) {
			Ref<PackedScene> packed_scene = ResourceLoader::get_singleton()->load(scene_path);
			if (packed_scene.is_valid()) {
				object = packed_scene->instantiate();
			}
		} else if (this->script.is_valid()) {
			object = script->call("new");
		}
	}

	ERR_FAIL_COND_V_MSG(!object, nullptr, "Failed to create or find object for deserialization of type: " + this->name);

	Ref<Script> instantiated_script = object->get_script();
	if (instantiated_script.is_null() || instantiated_script->get_path() != this->script->get_path()) {
		ERR_PRINT("Deserialization target object does not have the correct script attached. Expected: " + this->script->get_path());
	}

	p_context.target_object = object;
	if (Node *node = Object::cast_to<Node>(object)) {
		if (node->get_scene_file_path() != "") {
			p_context.scene_root_node = node;
		}
	}

	if (object->has_method("_deserialize")) {
		object->call("_deserialize", p_serialized);
		return object;
	}

	return _default_deserialize(p_serialized, p_context);
}

Dictionary NodeSerializer::ObjectRegistration::_default_serialize(Object *p_object, SerializationContext &p_context) const {
	Dictionary result;
	int required_property_usage_flags = p_context.required_property_usage_flags;

	for (const auto &[property_name, property_info] : _get_property_map(p_object)) {
		if (!(property_info.usage & required_property_usage_flags)) {
			continue;
		}

		if (property_info.has_custom_serializer) {
			result[property_name] = p_object->call(property_info.custom_serializer_name);
			continue;
		}

		Variant value = p_object->get(property_name);

		if (value == property_info.default_value) {
			continue;
		}

		if (value.get_type() == Variant::OBJECT) {
			Node *value_as_node = Object::cast_to<Node>(static_cast<Object *>(value));

			if (value_as_node) {
				if (p_context.scene_root_node) {
					result[property_name] = p_context.scene_root_node->get_path_to(value_as_node);
				}

				continue;
			}
		}

		StringName previous_property_name = p_context.property_name;
		p_context.property_name = property_name;
		result[property_name] = p_context.serialize_value(value, p_context);
		p_context.property_name = previous_property_name;
	}

	result[*FIELD_TYPE] = this->name;

	if (Node *node = Object::cast_to<Node>(p_object)) {
		String scene_path = node->get_scene_file_path();

		if (!scene_path.is_empty() && scene_path.begins_with("res://")) {
			result[*FIELD_SCENE] = scene_path;
		}

		Dictionary children_data = _serialize_children(node, p_context);

		if (!children_data.is_empty()) {
			result[*FIELD_CHILDREN] = children_data;
		}
	}

	return result;
}

Object *NodeSerializer::ObjectRegistration::_default_deserialize(const Dictionary &p_serialized, DeserializationContext &p_context) const {
	Object *object = p_context.target_object;
	ERR_FAIL_COND_V(!object, nullptr);

	Array keys = p_serialized.keys();
	for (int i = 0; i < keys.size(); ++i) {
		StringName key = keys[i];
		if (key == *FIELD_TYPE || key == *FIELD_SCENE || key == *FIELD_CHILDREN) {
			continue;
		}

		Variant value = p_serialized[key];

		const auto &property_list = _get_property_map(object);
		auto prop_info_it = property_list.find(key);

		if (prop_info_it != property_list.end() && prop_info_it->second.has_custom_deserializer) {
			object->call(prop_info_it->second.custom_deserializer_name, value);
			continue;
		}

		Variant deserialized_value = p_context.deserialize_value(value, p_context);

		bool is_object_prop = false;
		if (prop_info_it != property_list.end()) {
			if (prop_info_it->second.type == Variant::OBJECT) {
				is_object_prop = true;
			}
		}

		if (deserialized_value.get_type() == Variant::STRING && is_object_prop) {
			String path = deserialized_value;
			Node *scene_root_node = p_context.scene_root_node;

			if (!scene_root_node) {
				ERR_PRINT("Unable to deserialize object node path property without scene root node: " + name + "/" + key);
				continue;
			}

			auto node = scene_root_node->get_node_or_null(path);

			if (!node) {
				ERR_PRINT("Failed to resolve object for node path property " + name + ":" + key + ": " + path);
				continue;
			}

			object->set(key, node);
		} else {
			object->set(key, deserialized_value);
		}
	}

	if (Node *node = Object::cast_to<Node>(object)) {
		if (p_serialized.has(*FIELD_CHILDREN)) {
			_deserialize_children(node, p_serialized[*FIELD_CHILDREN], p_context);
		}
	}

	return object;
}
