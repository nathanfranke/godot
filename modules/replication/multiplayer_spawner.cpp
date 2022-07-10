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

#include "multiplayer_synchronizer.h"

void MultiplayerSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("__internal_rpc_spawn"), &MultiplayerSpawner::__internal_rpc_spawn);
	ClassDB::bind_method(D_METHOD("__internal_rpc_despawn"), &MultiplayerSpawner::__internal_rpc_despawn);

	ClassDB::bind_method(D_METHOD("get_spawn_path"), &MultiplayerSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &MultiplayerSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ClassDB::bind_method(D_METHOD("get_spawn_limit"), &MultiplayerSpawner::get_spawn_limit);
	ClassDB::bind_method(D_METHOD("set_spawn_limit", "limit"), &MultiplayerSpawner::set_spawn_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_limit", PROPERTY_HINT_RANGE, "0,1024,1,or_greater"), "set_spawn_limit", "get_spawn_limit");

	ClassDB::bind_method(D_METHOD("get_spawnable_scenes"), &MultiplayerSpawner::get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("set_spawnable_scenes", "scenes"), &MultiplayerSpawner::set_spawnable_scenes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "spawnable_scenes", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), PROPERTY_USAGE_STORAGE), "set_spawnable_scenes", "get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("spawn_custom", "custom_data"), &MultiplayerSpawner::spawn_custom, DEFVAL(Variant()));

	GDVIRTUAL_BIND(_spawn_custom, "custom_data");

	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::INT, "scene_index"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::INT, "scene_index"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void MultiplayerSpawner::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			_update_spawn_node();

			get_multiplayer()->connect(SNAME("peer_connected"), callable_mp(this, &MultiplayerSpawner::_on_peer_connected));
			get_multiplayer()->connect(SNAME("peer_disconnected"), callable_mp(this, &MultiplayerSpawner::_on_peer_disconnected));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			get_multiplayer()->disconnect(SNAME("peer_connected"), callable_mp(this, &MultiplayerSpawner::_on_peer_connected));
			get_multiplayer()->disconnect(SNAME("peer_disconnected"), callable_mp(this, &MultiplayerSpawner::_on_peer_disconnected));

			// Despawn all spawned nodes. Duplicate the list since _despawn_tracked_node will remove elements.
			for (Node *node : spawned_nodes.duplicate()) {
				const SpawnInfo *info = tracked_nodes.getptr(node);
				if (info == nullptr) {
					continue;
				}

				if (node->is_connected(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSpawner::_spawn_tracked_node))) {
					node->disconnect(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSpawner::_spawn_tracked_node));
				}
				node->disconnect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &MultiplayerSpawner::_despawn_tracked_node));

				_despawn_tracked_node(node);
			}
		} break;
	}
}

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

void MultiplayerSpawner::_update_spawn_node() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif

	if (spawn_node != nullptr) {
		spawn_node->disconnect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_on_child_added));
	}

	spawn_node = is_inside_tree() ? get_node_or_null(spawn_path) : nullptr;

	if (spawn_node != nullptr) {
		Error err = spawn_node->connect("child_entered_tree", callable_mp(this, &MultiplayerSpawner::_on_child_added));
		ERR_FAIL_COND(err != OK);
	}
}

void MultiplayerSpawner::_on_child_added(Node *p_node) {
	if (!get_multiplayer()->has_multiplayer_peer() ||
			get_multiplayer()->get_multiplayer_peer()->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED ||
			!is_multiplayer_authority()) {
		return;
	}
	if (tracked_nodes.has(p_node)) {
		return;
	}

	for (int scene_index = 0; scene_index < spawnable_scenes.size(); ++scene_index) {
		const Ref<PackedScene> scene = spawnable_scenes[scene_index];
		if (scene.is_null()) {
			continue;
		}

		if (scene->get_path() == p_node->get_scene_file_path()) {
			_track(p_node, scene_index, Variant());
			break;
		}
	}
}

void MultiplayerSpawner::_create_spawn_payloads(Node *p_node, Array &r_payloads) const {
	for (int i = 0; i < p_node->get_child_count(); ++i) {
		Node *child = p_node->get_child(i);

		MultiplayerSynchronizer *sync = Object::cast_to<MultiplayerSynchronizer>(child);
		if (sync != nullptr) {
			// Do not re-send sync when synchronizer is ready.
			sync->spawn_synced = true;

			Array payload;
			sync->_create_payload(MultiplayerSynchronizer::READY, payload);

			// Always add, even if an empty payload.
			r_payloads.push_back(payload);
		}

		_create_spawn_payloads(child, r_payloads);
	}
}

int MultiplayerSpawner::_apply_spawn_payloads(Node *p_node, const Array &p_payloads, int p_index) const {
	for (int i = 0; i < p_node->get_child_count(); ++i) {
		Node *child = p_node->get_child(i);

		MultiplayerSynchronizer *sync = Object::cast_to<MultiplayerSynchronizer>(child);
		if (sync != nullptr) {
			sync->connect(SceneStringNames::get_singleton()->ready, callable_mp(sync, &MultiplayerSynchronizer::_apply_payload), varray(p_payloads[p_index++]), CONNECT_ONESHOT);
		}

		p_index = _apply_spawn_payloads(child, p_payloads, p_index);
	}

	return p_index;
}

void MultiplayerSpawner::_track(Node *p_node, const int p_scene_index, const Variant &p_custom_data) {
	ERR_FAIL_COND_MSG(tracked_nodes.has(p_node), "Node is already being tracked.");

	p_node->connect(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSpawner::_spawn_tracked_node), varray(p_node, p_scene_index, p_custom_data), CONNECT_ONESHOT);
	p_node->connect(SceneStringNames::get_singleton()->tree_exiting, callable_mp(this, &MultiplayerSpawner::_despawn_tracked_node), varray(p_node), CONNECT_ONESHOT);
}

void MultiplayerSpawner::_spawn_tracked_node(Node *p_node, const int p_scene_index, const Variant &p_custom_data) {
	tracked_nodes[p_node] = SpawnInfo{
		.scene_index = p_scene_index,
		.custom_data = p_custom_data,
	};

	spawned_nodes.push_back(p_node);

	Array payloads;
	_create_spawn_payloads(p_node, payloads);

	rpc(SNAME("__internal_rpc_spawn"), p_node->get_name(), p_scene_index, p_custom_data, payloads);
}

void MultiplayerSpawner::_despawn_tracked_node(Node *p_node) {
	bool erased = tracked_nodes.erase(p_node);
	ERR_FAIL_COND(!erased);

	const int index = spawned_nodes.find(p_node);
	ERR_FAIL_COND(index == -1);

	spawned_nodes.remove_at(index);

	rpc(SNAME("__internal_rpc_despawn"), index);
}

void MultiplayerSpawner::_on_peer_connected(const int p_peer) {
	for (Node *node : spawned_nodes) {
		const SpawnInfo *info = tracked_nodes.getptr(node);
		if (info == nullptr) {
			continue;
		}

		Array payloads;
		_create_spawn_payloads(node, payloads);

		rpc_id(p_peer, SNAME("__internal_rpc_spawn"), node->get_name(), info->scene_index, info->custom_data, payloads);
	}
}

void MultiplayerSpawner::_on_peer_disconnected(const int p_peer) {
}

void MultiplayerSpawner::__internal_rpc_spawn(const String &p_name, const int p_scene_index, const Variant &p_custom_data, const Array &p_payloads) {
	Node *node;
	if (p_scene_index != CUSTOM_SCENE_INDEX) {
		node = instantiate_scene(p_scene_index);
	} else {
		node = instantiate_custom(p_custom_data);
	}

	int index = _apply_spawn_payloads(node, p_payloads);
	ERR_FAIL_COND_MSG(index != p_payloads.size(), "Spawn synchronization failed. The server and client have different configurations!");

	node->set_name(p_name);
	spawn_node->add_child(node);

	spawned_nodes.push_back(node);
}

void MultiplayerSpawner::__internal_rpc_despawn(const int p_index) {
	ERR_FAIL_INDEX(p_index, spawned_nodes.size());

	Node *node = spawned_nodes[p_index];
	ERR_FAIL_NULL(node);

	spawned_nodes.remove_at(p_index);

	spawn_node->remove_child(node);
	node->queue_delete();
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

void MultiplayerSpawner::set_spawnable_scenes(const Array &p_scenes) {
	spawnable_scenes = p_scenes;
}

Array MultiplayerSpawner::get_spawnable_scenes() const {
	return spawnable_scenes;
}

Node *MultiplayerSpawner::spawn_custom(const Variant &p_custom_data) {
	ERR_FAIL_COND_V(!is_inside_tree() || !get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority(), nullptr);
	ERR_FAIL_COND_V_MSG(spawn_limit > 0 && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_NULL_V_MSG(spawn_node, nullptr, "Cannot find spawn node.");

	Node *node = instantiate_custom(p_custom_data);
	ERR_FAIL_NULL_V(node, nullptr);

	_track(node, CUSTOM_SCENE_INDEX, p_custom_data);
	spawn_node->add_child(node, true);
	return node;
}

Node *MultiplayerSpawner::instantiate_scene(int p_scene_index) {
	ERR_FAIL_COND_V_MSG(spawn_limit > 0 && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_INDEX_V(p_scene_index, spawnable_scenes.size(), nullptr);
	const Ref<PackedScene> scene = spawnable_scenes[p_scene_index];
	ERR_FAIL_COND_V(scene.is_null(), nullptr);
	return scene->instantiate();
}

Node *MultiplayerSpawner::instantiate_custom(const Variant &p_custom_data) {
	ERR_FAIL_COND_V_MSG(spawn_limit > 0 && spawn_limit <= tracked_nodes.size(), nullptr, "Spawn limit reached!");
	ERR_FAIL_COND_V_MSG(!GDVIRTUAL_IS_OVERRIDDEN(_spawn_custom), nullptr, "Custom spawn requires the '_spawn_custom' virtual method to be implemented via script.");
	Node *node = nullptr;
	GDVIRTUAL_CALL(_spawn_custom, p_custom_data, node);
	ERR_FAIL_NULL_V_MSG(node, nullptr, "The '_spawn_custom' implementation must return a valid Node.");
	return node;
}

MultiplayerSpawner::MultiplayerSpawner() {
	rpc_config(SNAME("__internal_rpc_spawn"), RPCMode::RPC_MODE_AUTHORITY);
	rpc_config(SNAME("__internal_rpc_despawn"), RPCMode::RPC_MODE_AUTHORITY);
}
