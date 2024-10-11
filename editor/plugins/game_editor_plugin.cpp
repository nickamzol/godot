/**************************************************************************/
/*  game_editor_plugin.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "game_editor_plugin.h"

#include "core/config/project_settings.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/panel.h"

void GameEditorDebugger::_session_started(Ref<EditorDebuggerSession> p_session) {
	p_session->send_message("scene:runtime_node_select_setup", Array());

	Array type;
	type.append(node_type);
	p_session->send_message("scene:runtime_node_select_set_type", type);
	Array visible;
	visible.append(selection_visible);
	p_session->send_message("scene:runtime_node_select_set_visible", visible);
	Array mode;
	mode.append(select_mode);
	p_session->send_message("scene:runtime_node_select_set_mode", mode);

	emit_signal(SNAME("session_started"));
}

void GameEditorDebugger::_session_stopped() {
	emit_signal(SNAME("session_stopped"));
}

void GameEditorDebugger::set_suspend(bool p_enabled) {
	Array message;
	message.append(p_enabled);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:suspend_changed", message);
		}
	}
}

void GameEditorDebugger::next_frame() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:next_frame", Array());
		}
	}
}

void GameEditorDebugger::set_node_type(int p_type) {
	node_type = p_type;

	Array message;
	message.append(p_type);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_type", message);
		}
	}
}

void GameEditorDebugger::set_selection_visible(bool p_visible) {
	selection_visible = p_visible;

	Array message;
	message.append(p_visible);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_visible", message);
		}
	}
}

void GameEditorDebugger::set_select_mode(int p_mode) {
	select_mode = p_mode;

	Array message;
	message.append(p_mode);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_mode", message);
		}
	}
}

void GameEditorDebugger::set_camera_override(bool p_enabled) {
	EditorDebuggerNode::get_singleton()->set_camera_override(p_enabled ? camera_override_mode : EditorDebuggerNode::OVERRIDE_NONE);
}

void GameEditorDebugger::set_camera_manipulate_mode(EditorDebuggerNode::CameraOverride p_mode) {
	camera_override_mode = p_mode;

	if (EditorDebuggerNode::get_singleton()->get_camera_override() != EditorDebuggerNode::OVERRIDE_NONE) {
		set_camera_override(true);
	}
}

void GameEditorDebugger::reset_camera_2d_position() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_reset_camera_2d", Array());
		}
	}
}

void GameEditorDebugger::reset_camera_3d_position() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_reset_camera_3d", Array());
		}
	}
}

void GameEditorDebugger::setup_session(int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());

	sessions.append(session);

	session->connect("started", callable_mp(this, &GameEditorDebugger::_session_started).bind(session));
	session->connect("stopped", callable_mp(this, &GameEditorDebugger::_session_stopped));
}

void GameEditorDebugger::_bind_methods() {
	ADD_SIGNAL(MethodInfo("session_started"));
	ADD_SIGNAL(MethodInfo("session_stopped"));
}

///////

GameEditor *GameEditor::singleton = nullptr;

GameEditor *GameEditor::get_singleton() {
	return singleton;
}

void GameEditor::_sessions_changed() {
	// The debugger session's `session_started/stopped` signal can be unreliable, so count it manually.
	active_sessions = 0;
	Array sessions = debugger->get_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (Object::cast_to<EditorDebuggerSession>(sessions[i])->is_active()) {
			active_sessions++;
		}
	}

	_update_debugger_buttons();
}

void GameEditor::_play_pressed() {
	OS::ProcessID current_process_id = EditorRunBar::get_singleton()->get_current_process();
	if (current_process_id == 0) {
		return;
	}

	if (embedded_button->is_pressed()) {
		_update_embed_window_size();
		embedded_process->embed_process(current_process_id);
		_update_ui();

		if (auto_focus_button->is_pressed()) {
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_GAME);
		}
	}
}

void GameEditor::_stop_pressed() {
	embedded_process->reset();
	_update_ui();
}

void GameEditor::_embedding_completed() {
	_update_ui();
}

void GameEditor::_embedding_failed() {
	state_label->set_text(TTR("Connection impossible to the game process."));
}

void GameEditor::_project_settings_changed() {
	// Catch project settings changed to update window size/aspect ratio.
	_update_embed_window_size();
}

void GameEditor::_update_debugger_buttons() {
	bool empty = active_sessions == 0;

	suspend_button->set_disabled(empty);
	camera_override_button->set_disabled(empty);

	PopupMenu *menu = camera_override_menu->get_popup();

	bool disable_camera_reset = empty || !camera_override_button->is_pressed() || !menu->is_item_checked(menu->get_item_index(CAMERA_MODE_INGAME));
	menu->set_item_disabled(CAMERA_RESET_2D, disable_camera_reset);
	menu->set_item_disabled(CAMERA_RESET_3D, disable_camera_reset);

	if (empty) {
		suspend_button->set_pressed(false);
		camera_override_button->set_pressed(false);
	}
	next_frame_button->set_disabled(!suspend_button->is_pressed());
}

void GameEditor::_suspend_button_toggled(bool p_pressed) {
	_update_debugger_buttons();

	debugger->set_suspend(p_pressed);
}

void GameEditor::_node_type_pressed(int p_option) {
	RuntimeNodeSelect::NodeType type = (RuntimeNodeSelect::NodeType)p_option;
	for (int i = 0; i < RuntimeNodeSelect::NODE_TYPE_MAX; i++) {
		node_type_button[i]->set_pressed(i == type);
	}

	_update_debugger_buttons();

	debugger->set_node_type(type);
}

void GameEditor::_select_mode_pressed(int p_option) {
	RuntimeNodeSelect::SelectMode mode = (RuntimeNodeSelect::SelectMode)p_option;
	for (int i = 0; i < RuntimeNodeSelect::SELECT_MODE_MAX; i++) {
		select_mode_button[i]->set_pressed(i == mode);
	}

	debugger->set_select_mode(mode);
}

void GameEditor::_embedded_button_pressed() {
	EditorSettings::get_singleton()->set_project_metadata("game_editor", "embedded", embedded_button->is_pressed());

	if (EditorRunBar::get_singleton()->is_playing()) {
		EditorRunBar::get_singleton()->restart();
	}

	_update_ui();
}

void GameEditor::_auto_focus_button_pressed() {
	EditorSettings::get_singleton()->set_project_metadata("game_editor", "auto_focus", auto_focus_button->is_pressed());
}

void GameEditor::_keep_aspect_button_pressed() {
	EditorSettings::get_singleton()->set_project_metadata("game_editor", "keep_aspect", keep_aspect_button->is_pressed());
	embedded_process->set_keep_aspect(keep_aspect_button->is_pressed());
}

void GameEditor::_update_ui() {
	if (!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
		state_label->set_text(TTR("Game embedding not available on your OS."));
	} else if (embedded_process->is_embedding_completed()) {
		state_label->set_text("");
	} else if (embedded_process->is_embedding_in_progress()) {
		state_label->set_text(TTR("Game starting..."));
	} else if (EditorRunBar::get_singleton()->is_playing()) {
		state_label->set_text(TTR("Game running not embedded."));
	} else if (embedded_button->is_pressed()) {
		state_label->set_text(TTR("Press play to start the game."));
	} else {
		state_label->set_text(TTR("Embedding is disabled."));
	}
}

void GameEditor::_update_embed_window_size() {
	Size2 window_size;
	window_size.x = GLOBAL_GET("display/window/size/viewport_width");
	window_size.y = GLOBAL_GET("display/window/size/viewport_height");

	Size2 desired_size;
	desired_size.x = GLOBAL_GET("display/window/size/window_width_override");
	desired_size.y = GLOBAL_GET("display/window/size/window_height_override");
	if (desired_size.x > 0 && desired_size.y > 0) {
		window_size = desired_size;
	}
	embedded_process->set_window_size(window_size);
}

void GameEditor::_hide_selection_toggled(bool p_pressed) {
	hide_selection->set_icon(get_editor_theme_icon(p_pressed ? SNAME("GuiVisibilityHidden") : SNAME("GuiVisibilityVisible")));

	debugger->set_selection_visible(!p_pressed);
}

void GameEditor::_camera_override_button_toggled(bool p_pressed) {
	_update_debugger_buttons();

	debugger->set_camera_override(p_pressed);
}

void GameEditor::_camera_override_menu_id_pressed(int p_id) {
	PopupMenu *menu = camera_override_menu->get_popup();
	if (p_id != CAMERA_RESET_2D && p_id != CAMERA_RESET_3D) {
		for (int i = 0; i < menu->get_item_count(); i++) {
			menu->set_item_checked(i, false);
		}
	}

	switch (p_id) {
		case CAMERA_RESET_2D: {
			debugger->reset_camera_2d_position();
		} break;
		case CAMERA_RESET_3D: {
			debugger->reset_camera_3d_position();
		} break;
		case CAMERA_MODE_INGAME: {
			debugger->set_camera_manipulate_mode(EditorDebuggerNode::OVERRIDE_INGAME);
			menu->set_item_disabled(CAMERA_RESET_2D, false);
			menu->set_item_disabled(CAMERA_RESET_3D, false);
			menu->set_item_checked(menu->get_item_index(p_id), true);
		} break;
		case CAMERA_MODE_EDITORS: {
			debugger->set_camera_manipulate_mode(EditorDebuggerNode::OVERRIDE_EDITORS);
			menu->set_item_disabled(CAMERA_RESET_2D, true);
			menu->set_item_disabled(CAMERA_RESET_3D, true);
			menu->set_item_checked(menu->get_item_index(p_id), true);
		} break;
	}
}

void GameEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			suspend_button->set_icon(get_editor_theme_icon(SNAME("Pause")));
			next_frame_button->set_icon(get_editor_theme_icon(SNAME("NextFrame")));

			node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_icon(get_editor_theme_icon(SNAME("InputEventJoypadMotion")));
			node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_icon(get_editor_theme_icon(SNAME("2DNodes")));
#ifndef _3D_DISABLED
			node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_icon(get_editor_theme_icon(SNAME("Node3D")));
#endif // _3D_DISABLED

			select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_icon(get_editor_theme_icon(SNAME("ToolSelect")));
			select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_icon(get_editor_theme_icon(SNAME("ListSelect")));

			embedded_button->set_icon(get_editor_theme_icon(SNAME("EmbeddedProcess")));
			auto_focus_button->set_icon(get_editor_theme_icon(SNAME("AutoFocus")));
			keep_aspect_button->set_icon(get_editor_theme_icon(SNAME("KeepAspect")));

			hide_selection->set_icon(get_editor_theme_icon(hide_selection->is_pressed() ? SNAME("GuiVisibilityHidden") : SNAME("GuiVisibilityVisible")));

			camera_override_button->set_icon(get_editor_theme_icon(SNAME("Camera")));
			camera_override_menu->set_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));

			panel->set_theme_type_variation("GamePanel");
		} break;

		case NOTIFICATION_READY: {
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
				// Embedding available.
				embedded_button->set_pressed(EditorSettings::get_singleton()->get_project_metadata("game_editor", "embedded", true));
				auto_focus_button->set_pressed(EditorSettings::get_singleton()->get_project_metadata("game_editor", "auto_focus", true));
				keep_aspect_button->set_pressed(EditorSettings::get_singleton()->get_project_metadata("game_editor", "keep_aspect", true));

				EditorRunBar::get_singleton()->connect("play_pressed", callable_mp(this, &GameEditor::_play_pressed));
				EditorRunBar::get_singleton()->connect("stop_pressed", callable_mp(this, &GameEditor::_stop_pressed));

				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &GameEditor::_project_settings_changed));

				embedded_process->set_keep_aspect(keep_aspect_button->is_pressed());
			} else {
				// Embedding not available.
				embedding_separator->hide();
				embedded_button->hide();
				auto_focus_button->hide();
				keep_aspect_button->hide();
				keep_aspect_button->hide();
			}

			_update_ui();
		} break;
	}
}

void GameEditor::get_argument_list_for_instance(int p_idx, List<String> &r_list) {
	if (p_idx != 0 || !embedded_button->is_pressed() || !DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
		return;
	}

	// Remove duplicates/unwanted parameters.
	List<String, DefaultAllocator>::Element *position_item = r_list.find("--position");
	if (position_item) {
		r_list.erase(position_item->next());
		r_list.erase(position_item);
	}
	List<String, DefaultAllocator>::Element *resolution_item = r_list.find("--resolution");
	if (resolution_item) {
		r_list.erase(resolution_item->next());
		r_list.erase(resolution_item);
	}
	List<String, DefaultAllocator>::Element *screen_item = r_list.find("--screen");
	if (screen_item) {
		r_list.erase(screen_item->next());
		r_list.erase(screen_item);
	}
	r_list.erase("-f");
	r_list.erase("--fullscreen");
	r_list.erase("-m");
	r_list.erase("--maximized");
	r_list.erase("-t");
	r_list.erase("--always-on-top");
	r_list.erase("--hidden");

	// Add editor window native id so the started game can directly set it's parent to it.
	r_list.push_back("--wid");
	r_list.push_back(itos(DisplayServer::get_singleton()->window_get_native_handle(DisplayServer::WINDOW_HANDLE, get_window()->get_window_id())));

	if (!embedded_process->is_visible_in_tree() && !auto_focus_button->is_pressed()) {
		r_list.push_back("--hidden");
	}

	// Be sure to have the correct window size in the embedded_process control.
	_update_embed_window_size();

	Rect2i rect = embedded_process->get_screen_embedded_window_rect();
	r_list.push_back("--position");
	r_list.push_back(itos(rect.position.x) + "," + itos(rect.position.y));
	r_list.push_back("--resolution");
	r_list.push_back(itos(rect.size.x) + "x" + itos(rect.size.y));
}

GameEditor::GameEditor(Ref<GameEditorDebugger> p_debugger) {
	singleton = this;
	debugger = p_debugger;

	// Add some margin to the sides for better aesthetics.
	// This prevents the first button's hover/pressed effect from "touching" the panel's border,
	// which looks ugly.
	MarginContainer *toolbar_margin = memnew(MarginContainer);
	toolbar_margin->add_theme_constant_override("margin_left", 4 * EDSCALE);
	toolbar_margin->add_theme_constant_override("margin_right", 4 * EDSCALE);
	add_child(toolbar_margin);

	HBoxContainer *main_menu_hbox = memnew(HBoxContainer);
	toolbar_margin->add_child(main_menu_hbox);

	suspend_button = memnew(Button);
	main_menu_hbox->add_child(suspend_button);
	suspend_button->set_toggle_mode(true);
	suspend_button->set_theme_type_variation("FlatButton");
	suspend_button->connect(SceneStringName(toggled), callable_mp(this, &GameEditor::_suspend_button_toggled));
	suspend_button->set_tooltip_text(TTR("Suspend"));

	next_frame_button = memnew(Button);
	main_menu_hbox->add_child(next_frame_button);
	next_frame_button->set_theme_type_variation("FlatButton");
	next_frame_button->connect(SceneStringName(pressed), callable_mp(*debugger, &GameEditorDebugger::next_frame));
	next_frame_button->set_tooltip_text(TTR("Next Frame"));

	main_menu_hbox->add_child(memnew(VSeparator));

	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_text(TTR("Input"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_pressed(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_theme_type_variation("FlatButton");
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_NONE));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_tooltip_text(TTR("Allow game input."));

	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_text(TTR("2D"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_theme_type_variation("FlatButton");
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_2D));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_tooltip_text(TTR("Disable game input and allow to select Node2Ds, Controls, and manipulate the 2D camera."));

#ifndef _3D_DISABLED
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_text(TTR("3D"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_theme_type_variation("FlatButton");
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_3D));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_tooltip_text(TTR("Disable game input and allow to select Node3Ds and manipulate the 3D camera."));
#endif // _3D_DISABLED

	main_menu_hbox->add_child(memnew(VSeparator));

	hide_selection = memnew(Button);
	main_menu_hbox->add_child(hide_selection);
	hide_selection->set_toggle_mode(true);
	hide_selection->set_theme_type_variation("FlatButton");
	hide_selection->connect(SceneStringName(toggled), callable_mp(this, &GameEditor::_hide_selection_toggled));
	hide_selection->set_tooltip_text(TTR("Toggle Selection Visibility"));

	main_menu_hbox->add_child(memnew(VSeparator));

	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE] = memnew(Button);
	main_menu_hbox->add_child(select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_toggle_mode(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_pressed(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_theme_type_variation("FlatButton");
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_select_mode_pressed).bind(RuntimeNodeSelect::SELECT_MODE_SINGLE));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_select", TTR("Select Mode"), Key::Q));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_shortcut_context(this);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Alt+RMB: Show list of all nodes at position clicked."));

	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST] = memnew(Button);
	main_menu_hbox->add_child(select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_toggle_mode(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_theme_type_variation("FlatButton");
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_select_mode_pressed).bind(RuntimeNodeSelect::SELECT_MODE_LIST));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_tooltip_text(TTR("Show list of selectable nodes at position clicked."));

	main_menu_hbox->add_child(memnew(VSeparator));

	camera_override_button = memnew(Button);
	main_menu_hbox->add_child(camera_override_button);
	camera_override_button->set_toggle_mode(true);
	camera_override_button->set_theme_type_variation("FlatButton");
	camera_override_button->connect(SceneStringName(toggled), callable_mp(this, &GameEditor::_camera_override_button_toggled));
	camera_override_button->set_tooltip_text(TTR("Override the in-game camera."));

	camera_override_menu = memnew(MenuButton);
	main_menu_hbox->add_child(camera_override_menu);
	camera_override_menu->set_flat(false);
	camera_override_menu->set_theme_type_variation("FlatMenuButton");
	camera_override_menu->set_h_size_flags(SIZE_SHRINK_END);
	camera_override_menu->set_tooltip_text(TTR("Camera Override Options"));

	PopupMenu *menu = camera_override_menu->get_popup();
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &GameEditor::_camera_override_menu_id_pressed));
	menu->add_item(TTR("Reset 2D Position"), CAMERA_RESET_2D);
	menu->add_item(TTR("Reset 3D Position"), CAMERA_RESET_3D);
	menu->add_separator();
	menu->add_radio_check_item(TTR("Manipulate In-Game"), CAMERA_MODE_INGAME);
	menu->set_item_checked(menu->get_item_index(CAMERA_MODE_INGAME), true);
	menu->add_radio_check_item(TTR("Manipulate From Editors"), CAMERA_MODE_EDITORS);

	embedding_separator = memnew(VSeparator);
	main_menu_hbox->add_child(embedding_separator);

	embedded_button = memnew(Button);
	main_menu_hbox->add_child(embedded_button);
	embedded_button->set_toggle_mode(true);
	embedded_button->set_theme_type_variation("FlatButton");
	embedded_button->set_tooltip_text(TTR("Activate the game embedding mode."));
	embedded_button->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_embedded_button_pressed));

	auto_focus_button = memnew(Button);
	main_menu_hbox->add_child(auto_focus_button);
	auto_focus_button->set_toggle_mode(true);
	auto_focus_button->set_theme_type_variation("FlatButton");
	auto_focus_button->set_tooltip_text(TTR("Focus the game editor on project run."));
	auto_focus_button->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_auto_focus_button_pressed));

	keep_aspect_button = memnew(Button);
	main_menu_hbox->add_child(keep_aspect_button);
	keep_aspect_button->set_toggle_mode(true);
	keep_aspect_button->set_theme_type_variation("FlatButton");
	keep_aspect_button->set_tooltip_text(TTR("Keep aspect ratio of the embedded game."));
	keep_aspect_button->connect(SceneStringName(pressed), callable_mp(this, &GameEditor::_keep_aspect_button_pressed));

	panel = memnew(Panel);
	add_child(panel);
	panel->set_v_size_flags(SIZE_EXPAND_FILL);

	embedded_process = memnew(EmbeddedProcess);
	panel->add_child(embedded_process);
	embedded_process->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	embedded_process->connect(SNAME("embedding_failed"), callable_mp(this, &GameEditor::_embedding_failed));
	embedded_process->connect(SNAME("embedding_completed"), callable_mp(this, &GameEditor::_embedding_completed));

	state_label = memnew(Label());
	panel->add_child(state_label);
	state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	state_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	state_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	state_label->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	_update_debugger_buttons();

	p_debugger->connect("session_started", callable_mp(this, &GameEditor::_sessions_changed));
	p_debugger->connect("session_stopped", callable_mp(this, &GameEditor::_sessions_changed));
}

GameEditor::~GameEditor() {
	singleton = nullptr;
}

///////

void GameEditorPlugin::make_visible(bool p_visible) {
	game_editor->set_visible(p_visible);
}

void GameEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_debugger_plugin(debugger);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_debugger_plugin(debugger);
		} break;
	}
}

GameEditorPlugin::GameEditorPlugin() {
	debugger.instantiate();

	game_editor = memnew(GameEditor(debugger));
	game_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(game_editor);
	game_editor->hide();
}

GameEditorPlugin::~GameEditorPlugin() {
}
