/*************************************************************************/
/*  multiplayer_spawner.h                                                */
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

#ifndef MULTIPLAYER_SPAWNER_H
#define MULTIPLAYER_SPAWNER_H

#include "scene/main/node.h"

#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/scene_replication_config.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

public:
	enum {
		INVALID_SCENE_INDEX = 0xFFFFFFFF,
	};

private:
	struct SpawnInfo {
		int scene_index;
		Variant custom_data;
	};

	NodePath spawn_path;
	uint32_t spawn_limit = 0;
	bool auto_spawn = false;
	Array spawnable_scenes;

	Node *spawn_parent = nullptr;
	HashMap<Node *, SpawnInfo> tracked_nodes;

	void _update_spawn_node();
	void _on_child_added(Node *p_node);

	void _track(Node *p_node, int p_scene_index, const Variant &p_custom_data);
	void _on_tracked_ready(Node *p_node);
	void _on_tracked_exit(Node *p_node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

// Editor-only array properties.
#ifdef TOOLS_ENABLED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
#endif

public:
	NodePath get_spawn_path() const;
	void set_spawn_path(const NodePath &p_path);

	void set_spawn_limit(uint32_t p_limit);
	uint32_t get_spawn_limit() const;

	void set_auto_spawn(bool p_auto_spawn);
	bool get_auto_spawn() const;

	Array get_spawnable_scenes() const;
	void set_spawnable_scenes(const Array &p_scenes);

	int find_spawnable_scene_index_from_path(String p_path) const;
	int get_spawned_scene_index(Node *p_node) const;
	Variant get_spawned_custom_data(Node *p_node) const;

	Node *spawn(const Variant &p_data = Variant());
	Node *instantiate_custom(const Variant &p_data);
	Node *instantiate_scene(int p_idx);

	GDVIRTUAL1R(Object *, _spawn_custom, const Variant &);

	MultiplayerSpawner() {
		//rpc_config(SNAME("test"), RPCMode::RPC_MODE_AUTHORITY, true);
	}
};

#endif // MULTIPLAYER_SPAWNER_H
