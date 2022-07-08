/*************************************************************************/
/*  multiplayer_spawner.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "multiplayer_spawner.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/window.h"
#include "scene/scene_string_names.h"

// Editor-only array properties.
#ifdef TOOLS_ENABLED

bool MultiplayerSpawner::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "_spawnable_scene_count") {
		spawnable_scenes.resize(p_value);
		notify_property_list_changed();
		return true;
	}

	String name = p_name;
	if (name.begins_with("scenes/")) {
		int index = name.get_slicec('/', 1).to_int();
		ERR_FAIL_INDEX_V(index, spawnable_scenes.size(), false);
		spawnable_scenes.set(index, p_value);
		return true;
	}

	return false;
}

bool MultiplayerSpawner::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "_spawnable_scene_count") {
		r_ret = spawnable_scenes.size();
		return true;
	}

	String name = p_name;
	if (name.begins_with("scenes/")) {
		int index = name.get_slicec('/', 1).to_int();
		ERR_FAIL_INDEX_V(index, spawnable_scenes.size(), false);
		r_ret = spawnable_scenes.get(index);
		return true;
	}

	return false;
}

void MultiplayerSpawner::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "_spawnable_scene_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ARRAY, "Scenes,scenes/"));

	for (uint32_t i = 0; i < spawnable_scenes.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, "scenes/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "PackedScene", PROPERTY_USAGE_EDITOR));
	}
}

#endif

void MultiplayerSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_spawn_path"), &MultiplayerSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &MultiplayerSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ClassDB::bind_method(D_METHOD("get_spawn_limit"), &MultiplayerSpawner::get_spawn_limit);
	ClassDB::bind_method(D_METHOD("set_spawn_limit", "limit"), &MultiplayerSpawner::set_spawn_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_limit", PROPERTY_HINT_RANGE, "0,1024,1,or_greater"), "set_spawn_limit", "get_spawn_limit");

	ClassDB::bind_method(D_METHOD("set_auto_spawn", "auto_spawn"), &MultiplayerSpawner::set_auto_spawn);
	ClassDB::bind_method(D_METHOD("get_auto_spawn"), &MultiplayerSpawner::get_auto_spawn);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_spawn"), "set_auto_spawn", "get_auto_spawn");

	ClassDB::bind_method(D_METHOD("get_spawnable_scenes"), &MultiplayerSpawner::get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("set_spawnable_scenes", "scenes"), &MultiplayerSpawner::set_spawnable_scenes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "spawnable_scenes", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), PROPERTY_USAGE_STORAGE), "set_spawnable_scenes", "get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("get_spawned_scene_index"), &MultiplayerSpawner::get_spawned_scene_index);
	ClassDB::bind_method(D_METHOD("get_spawned_custom_data"), &MultiplayerSpawner::get_spawned_custom_data);

	ClassDB::bind_method(D_METHOD("instantiate_scene", "scene_index"), &MultiplayerSpawner::instantiate_scene);
	ClassDB::bind_method(D_METHOD("instantiate_custom", "custom_data"), &MultiplayerSpawner::instantiate_custom);

	ClassDB::bind_method(D_METHOD("spawn", "data"), &MultiplayerSpawner::spawn, DEFVAL(Variant()));

	GDVIRTUAL_BIND(_spawn_custom, "data");

	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void MultiplayerSpawner::_update_spawn_node() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif

	if (spawn_parent != nullptr) {
		spawn_parent->disconnect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_on_child_added));
	}

	spawn_parent = is_inside_tree() ? get_node_or_null(spawn_path) : nullptr;

	if (spawn_parent != nullptr) {
		Error err = spawn_parent->connect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_on_child_added));
		ERR_FAIL_COND(err != OK);
	}
}

void MultiplayerSpawner::_on_child_added(Node *p_node) {
	if (!get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority()) {
		return;
	}
	if (!auto_spawn) {
		return;
	}
	if (tracked_nodes.has(p_node)) {
		return;
	}

	const int scene_index = find_spawnable_scene_index_from_path(p_node->get_scene_file_path());
	if (scene_index == -1) {
		return;
	}

	const String name = p_node->get_name();
	ERR_FAIL_COND_MSG(name.validate_node_name() != name, vformat("Unable to auto-spawn node with invalid name: %s. Make sure to add your replicated nodes via 'add_child(node, true)' to produce valid names.", name));
	_track(p_node, scene_index, Variant());
}

void MultiplayerSpawner::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			_update_spawn_node();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_update_spawn_node();

			for (const KeyValue<Node *, SpawnInfo> &E : tracked_nodes) {
				Node *node = E.key;
				ERR_CONTINUE(!node);

				node->disconnect(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSpawner::_on_tracked_ready));
				node->disconnect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &MultiplayerSpawner::_on_tracked_exit));
				get_multiplayer()->despawn(node, this);
			}
			tracked_nodes.clear();
		} break;
	}
}

void MultiplayerSpawner::set_spawn_path(const NodePath &p_path) {
	spawn_path = p_path;
	_update_spawn_node();
}

NodePath MultiplayerSpawner::get_spawn_path() const {
	return spawn_path;
}

void MultiplayerSpawner::set_spawn_limit(uint32_t p_limit) {
	spawn_limit = p_limit;
}

uint32_t MultiplayerSpawner::get_spawn_limit() const {
	return spawn_limit;
}

void MultiplayerSpawner::set_auto_spawn(bool p_auto_spawn) {
	auto_spawn = p_auto_spawn;
}

bool MultiplayerSpawner::get_auto_spawn() const {
	return auto_spawn;
}

void MultiplayerSpawner::set_spawnable_scenes(const Array &p_scenes) {
	spawnable_scenes = p_scenes;
}

Array MultiplayerSpawner::get_spawnable_scenes() const {
	return spawnable_scenes;
}

void MultiplayerSpawner::_track(Node *p_node, int p_scene_index, const Variant &p_custom_data) {
	ERR_FAIL_COND_MSG(tracked_nodes.has(p_node), "Node is already being tracked.");

	tracked_nodes[p_node] = SpawnInfo{
		.scene_index = p_scene_index,
		.custom_data = p_custom_data,
	};

	p_node->connect(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSpawner::_on_tracked_ready), varray(p_node), CONNECT_ONESHOT);
	p_node->connect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &MultiplayerSpawner::_on_tracked_exit), varray(p_node), CONNECT_ONESHOT);
}

void MultiplayerSpawner::_on_tracked_ready(Node *p_node) {
	get_multiplayer()->spawn(p_node, this);
}

void MultiplayerSpawner::_on_tracked_exit(Node *p_node) {
	bool erased = tracked_nodes.erase(p_node);
	ERR_FAIL_COND(!erased);

	get_multiplayer()->despawn(p_node, this);
}

int MultiplayerSpawner::find_spawnable_scene_index_from_path(String p_path) const {
	for (int i = 0; i < spawnable_scenes.size(); ++i) {
		const Ref<PackedScene> scene = spawnable_scenes[i];
		if (scene.is_null()) {
			continue;
		}

		if (scene->get_path() == p_path) {
			return i;
		}
	}
	return -1;
}

int MultiplayerSpawner::get_spawned_scene_index(Node *p_node) const {
	const SpawnInfo *info = tracked_nodes.getptr(p_node);
	ERR_FAIL_NULL_V(info, -1);

	return info->scene_index;
}

Variant MultiplayerSpawner::get_spawned_custom_data(Node *p_node) const {
	const SpawnInfo *info = tracked_nodes.getptr(p_node);
	ERR_FAIL_NULL_V(info, Variant());

	return info->custom_data;
}

Node *MultiplayerSpawner::instantiate_scene(int p_scene_index) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_INDEX_V(p_scene_index, spawnable_scenes.size(), nullptr);
	const Ref<PackedScene> scene = spawnable_scenes[p_scene_index];
	ERR_FAIL_COND_V(scene.is_null(), nullptr);
	return scene->instantiate();
}

Node *MultiplayerSpawner::instantiate_custom(const Variant &p_custom_data) {
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	Object *obj = nullptr;
	Node *node = nullptr;
	if (GDVIRTUAL_CALL(_spawn_custom, p_custom_data, obj)) {
		node = Object::cast_to<Node>(obj);
	}
	return node;
}

Node *MultiplayerSpawner::spawn(const Variant &p_custom_data) {
	ERR_FAIL_COND_V(!is_inside_tree() || !get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority(), nullptr);
	ERR_FAIL_COND_V_MSG(spawn_limit && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!GDVIRTUAL_IS_OVERRIDDEN(_spawn_custom), nullptr, "Custom spawn requires the '_spawn_custom' virtual method to be implemented via script.");

	ERR_FAIL_NULL_V_MSG(spawn_parent, nullptr, "Cannot find spawn node.");

	Node *node = instantiate_custom(p_custom_data);
	ERR_FAIL_COND_V_MSG(!node, nullptr, "The '_spawn_custom' implementation must return a valid Node.");

	_track(node, INVALID_SCENE_INDEX, p_custom_data);
	spawn_parent->add_child(node, true);
	return node;
}
