#pragma once

#include <functional>
#include <map>
#include "godot_cpp/classes/script.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

class NodeSerializer : public Object {
	GDCLASS(NodeSerializer, Object);

public:
	static void initialize();
	static void cleanup();

	static void register_serializable_class(const Variant &p_script_or_path, bool p_mutable_property_list = false);

	static Variant serialize_to_json_structure(const Variant &p_value, const Dictionary &p_options = Dictionary());
	static String serialize_to_json(const Variant &p_value, const String &p_indent = "", bool p_sort_keys = false, bool p_full_precision = false, const Dictionary &p_options = Dictionary());
	static Variant deserialize_from_json_structure(const Variant &p_value, const Dictionary &p_options = Dictionary());
	static Variant deserialize_from_json(const String &p_json_string, const Dictionary &p_options = Dictionary());

	static Variant serialize_to_binary_structure(const Variant &p_value, const Dictionary &p_options = Dictionary());
	static PackedByteArray serialize_to_binary(const Variant &p_value, const Dictionary &p_options = Dictionary());
	static Variant deserialize_from_binary_structure(const Variant &p_value, const Dictionary &p_options = Dictionary());
	static Variant deserialize_from_binary(const PackedByteArray &p_bytes, const Dictionary &p_options = Dictionary());

private:
	NodeSerializer() = default;

	struct SerializationContext {
		std::function<Variant(const Variant &, SerializationContext &)> serialize_value;
		Object *target_object = nullptr;
		Node *scene_root_node = nullptr;
		int required_property_usage_flags = PROPERTY_USAGE_STORAGE;
		StringName property_name = StringName();
	};

	struct DeserializationContext {
		std::function<Variant(const Variant &, DeserializationContext &)> deserialize_value;
		Object *target_object = nullptr;
		Node *scene_root_node = nullptr;
	};

	struct PropertyCacheData {
		Variant::Type type = Variant::NIL;
		StringName name;
		uint32_t usage = PROPERTY_USAGE_DEFAULT;
		Variant default_value;
		StringName custom_serializer_name;
		bool has_custom_serializer = false;
		StringName custom_deserializer_name;
		bool has_custom_deserializer = false;
	};

	class ObjectRegistration {
	private:
		enum class HasMethod : uint8_t {
			Unchecked,
			No,
			Yes,
		};
	public:
		String name;
		Ref<Script> script;
		bool mutable_property_list;

		Dictionary serialize(Object *p_object, SerializationContext &p_context) const;
		Object *deserialize(const Dictionary &p_serialized, DeserializationContext &p_context) const;

	private:
		mutable std::map<StringName, PropertyCacheData> _cached_property_map;
		mutable HasMethod has_custom_serializer = HasMethod::Unchecked;

		const std::map<StringName, PropertyCacheData> &_get_property_map(Object *p_object) const;
		Dictionary _default_serialize(Object *p_object, SerializationContext &p_context) const;
		Object *_default_deserialize(const Dictionary &p_serialized, DeserializationContext &p_context) const;
	};

	static StringName *FIELD_CHILDREN;
	static StringName *FIELD_SCENE;
	static StringName *FIELD_TYPE;
	static StringName *TYPE_UNSERIALIZABLE_CHILD;
	static StringName *TYPE_NAME_NATIVE;
	static StringName *TYPE_NAME_PACKED_BYTE_ARRAY;
	static StringName *FIELD_DATA;
	static StringName *TYPE_NAME_TYPED_GDICTIONARY;
	static StringName *FIELD_ENTRIES;
	static StringName *FIELD_KEY_TYPE;
	static StringName *FIELD_KEY_CLASS_NAME;
	static StringName *FIELD_VALUE_TYPE;
	static StringName *FIELD_VALUE_CLASS_NAME;
	static StringName *PROPERTY_SCRIPT;
	static StringName *PROPERTY_RESOURCE_PATH;
	static StringName *METHOD_SERIALIZE;

	static HashMap<String, ObjectRegistration *> _script_registry;

	static void _apply_serialization_context_options(SerializationContext &p_context, const Dictionary &p_options);
	static void _apply_deserialization_context_options(DeserializationContext &p_context, const Dictionary &p_options);

	static Variant _serialize_recursively(const Variant &p_value, SerializationContext &p_context);
	static Variant _deserialize_recursively(const Variant &p_value, DeserializationContext &p_context);

	static Variant _json_serialize_value(const Variant &p_value, SerializationContext &p_context);
	static Variant _json_deserialize_value(const Variant &p_value, DeserializationContext &p_context);

	static String _get_object_registration_name(Object *p_object);
	static ObjectRegistration *_get_serializable_registration(const String &p_name);
	static Dictionary _serialize_children(Node *p_node, SerializationContext &p_context);
	static void _deserialize_children(Node *node, const Dictionary &p_serialized_children, DeserializationContext &p_context);

protected:
	static void _bind_methods();
};
