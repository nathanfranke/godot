/*************************************************************************/
/*  multiplayer_synchronizer.cpp                                         */
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

#include "multiplayer_synchronizer.h"

#include "core/config/engine.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/scene_string_names.h"

void MultiplayerSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("__internal_rpc_synchronize", "payload"), &MultiplayerSynchronizer::__internal_rpc_synchronize);

	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &MultiplayerSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &MultiplayerSynchronizer::get_root_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");

	ClassDB::bind_method(D_METHOD("set_replication_interval", "interval"), &MultiplayerSynchronizer::set_replication_interval);
	ClassDB::bind_method(D_METHOD("get_replication_interval"), &MultiplayerSynchronizer::get_replication_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "replication_interval", PROPERTY_HINT_RANGE, "0,5,0.001,suffix:s"), "set_replication_interval", "get_replication_interval");

	ClassDB::bind_method(D_METHOD("set_replication_config", "config"), &MultiplayerSynchronizer::set_replication_config);
	ClassDB::bind_method(D_METHOD("get_replication_config"), &MultiplayerSynchronizer::get_replication_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replication_config", PROPERTY_HINT_RESOURCE_TYPE, "SceneReplicationConfig"), "set_replication_config", "get_replication_config");
}

void MultiplayerSynchronizer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			set_process_internal(true);

			_update_root_node();

			if (root_node != nullptr) {
				print_line("synchronizer: connecting to ready signal of " + root_node->get_name());
				// Deferred signal to ensure the spawn was handled first.
				root_node->connect(SceneStringNames::get_singleton()->ready, callable_mp(this, &MultiplayerSynchronizer::_on_root_ready), varray(), CONNECT_DEFERRED | CONNECT_ONESHOT);
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			synchronize(SYNC);
		} break;
	}
}

bool MultiplayerSynchronizer::_get_path_target(const NodePath &p_path, Node *&r_node, StringName &r_prop) {
	if (p_path.get_name_count() != 0) {
		r_node = root_node->get_node(String(p_path.get_concatenated_names()));
	} else {
		r_node = root_node;
	}
	ERR_FAIL_NULL_V(r_node, false);

	r_prop = p_path.get_concatenated_subnames();
	return true;
}

void MultiplayerSynchronizer::_update_root_node() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif

	root_node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
}

Array MultiplayerSynchronizer::_create_payload(const SynchronizeAction p_action) {
	Array payload;

	switch (p_action) {
		case READY: {
			for (const NodePath &path : replication_config->get_spawn_properties()) {
				Node *node;
				StringName prop;
				const bool found_node = _get_path_target(path, node, prop);
				ERR_CONTINUE(!found_node);

				Variant value = node->get(prop);

				payload.push_back(path);
				payload.push_back(value);
			}
		} break;
		case SYNC: {
			for (const NodePath &path : replication_config->get_sync_properties()) {
				Node *node;
				StringName prop;
				const bool found_node = _get_path_target(path, node, prop);
				ERR_CONTINUE(!found_node);

				Variant value = node->get(prop);

				payload.push_back(path);
				payload.push_back(value);
			}
		} break;
	}

	return payload;
}

void MultiplayerSynchronizer::_apply_payload(const Array &p_payload) {
	// TODO: Validation
	for (int i = 0; i < p_payload.size();) {
		NodePath path = p_payload[i++];
		Variant value = p_payload[i++];

		Node *node;
		StringName prop;
		const bool found_node = _get_path_target(path, node, prop);
		ERR_CONTINUE(!found_node);

		node->set(prop, value);
	}
}

void MultiplayerSynchronizer::_on_root_ready() {
	if (spawn_synced) {
		print_line("Skipping spawn sync... already done!");
		return;
	}

	if (get_multiplayer()->has_multiplayer_peer() && is_multiplayer_authority()) {
		print_line("Synchronizer received root ready signal. SENDING SYNC RPC...");
	}
	synchronize(READY);
}

void MultiplayerSynchronizer::_on_peer_connected(const int p_peer) {
}

void MultiplayerSynchronizer::_on_peer_disconnected(const int p_peer) {
}

void MultiplayerSynchronizer::__internal_rpc_synchronize(const Array &p_payload) {
	print_line("Recieved synchronize RPC (Size: " + String(Variant(p_payload.size())) + ")");

	_apply_payload(p_payload);
}

void MultiplayerSynchronizer::set_root_path(const NodePath &p_path) {
	root_path = p_path;
}

NodePath MultiplayerSynchronizer::get_root_path() const {
	return root_path;
}

void MultiplayerSynchronizer::set_replication_interval(double p_interval) {
	ERR_FAIL_COND_MSG(p_interval < 0, "Replication interval must be greater or equal to 0.");
	replication_interval = p_interval;
}

double MultiplayerSynchronizer::get_replication_interval() const {
	return replication_interval;
}

void MultiplayerSynchronizer::set_replication_config(Ref<SceneReplicationConfig> p_config) {
	replication_config = p_config;
}

Ref<SceneReplicationConfig> MultiplayerSynchronizer::get_replication_config() {
	return replication_config;
}

void MultiplayerSynchronizer::synchronize(const SynchronizeAction p_action, const int p_peer) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif

	if (!get_multiplayer()->has_multiplayer_peer() || !is_multiplayer_authority()) {
		return;
	}

	ERR_FAIL_NULL(replication_config);

	Array payload = _create_payload(p_action);

	//print_line("Payload: " + String(Variant(payload)));
	if (!payload.is_empty()) {
		rpc_id(p_peer, SNAME("__internal_rpc_synchronize"), payload);
	}
}

MultiplayerSynchronizer::MultiplayerSynchronizer() {
	rpc_config(SNAME("__internal_rpc_synchronize"), RPCMode::RPC_MODE_AUTHORITY);
}
