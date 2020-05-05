#pragma once

#define _CRT_SECURE_NO_DEPRECATE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include <string>
#include <msclr\marshal_cppstd.h>

#include "../ModulateVivoxLibrary/ModulateVivoxLibrary.h"

using namespace System;

namespace ModulateVivoxWrapper {
	std::string undo_windows_system_string(String^ windows_string);
	String^ create_windows_system_string(const std::string& sane_string);
	void debug(const std::string& str);

	public ref class ModulateVivoxManagedWrapper
	{
	public:
		ModulateVivoxManagedWrapper(String^ log_dir) : 
			unmanaged_wrapper(new ModulateVivoxLibrary::UnmanagedWrapper(undo_windows_system_string(log_dir))) {}
		~ModulateVivoxManagedWrapper() {
			delete unmanaged_wrapper;
		}

		unsigned int get_number_of_skins() { return unmanaged_wrapper->get_number_of_skins(); }
		String^ get_voice_skin_name(int index) { return create_windows_system_string(unmanaged_wrapper->get_voice_skin_name(index)); }
		void select_voice_skin(String^ skin_name) { return unmanaged_wrapper->select_voice_skin(undo_windows_system_string(skin_name)); }

		int create_voice_skin(String^ filename) { return unmanaged_wrapper->create_voice_skin(undo_windows_system_string(filename)); }
		int load_api_key_from_file(String^ filename) { return unmanaged_wrapper->load_api_key_from_file(undo_windows_system_string(filename)); }
		String^ create_auth_message_for_voice_skin(String^ voice_skin_name) { return create_windows_system_string(unmanaged_wrapper->create_auth_message_for_voice_skin(undo_windows_system_string(voice_skin_name))); }
		int check_auth_message_for_voice_skin(String^ voice_skin_name, String^ auth_message) { return unmanaged_wrapper->check_auth_message_for_voice_skin(undo_windows_system_string(voice_skin_name), undo_windows_system_string(auth_message)); }

		void vivox_start_connect() { return unmanaged_wrapper->vivox_start_connect(); }
		int vivox_check_connected() { return unmanaged_wrapper->vivox_check_connected(); }
		void vivox_login() { return unmanaged_wrapper->vivox_login(); }
		void vivox_stop() { return unmanaged_wrapper->vivox_stop(); }
		int vivox_check_logged_in() { return unmanaged_wrapper->vivox_check_logged_in(); }
		void vivox_start_realtime_echo() { return unmanaged_wrapper->vivox_start_realtime_echo(); }
		void vivox_end_realtime_echo() { return unmanaged_wrapper->vivox_end_realtime_echo(); }
		void vivox_add_session(String^ channel_name) { return unmanaged_wrapper->vivox_add_session(undo_windows_system_string(channel_name)); }
		void vivox_remove_session(String^ channel_name) { return unmanaged_wrapper->vivox_remove_session(undo_windows_system_string(channel_name)); }

		void set_radio_strength(float value) { return unmanaged_wrapper->set_radio_strength(value); }
		void set_presence_strength(float value) { return unmanaged_wrapper->set_presence_strength(value); }
		void set_bass_booster_strength(float value) { return unmanaged_wrapper->set_bass_booster_strength(value); }
		void set_intimidator_strength(float value) { return unmanaged_wrapper->set_intimidator_strength(value); }
		void set_helm_strength(float value) { return unmanaged_wrapper->set_helm_strength(value); }
		void set_vivid_strength(float value) { return unmanaged_wrapper->set_vivid_strength(value); }

		unsigned int version() { return unmanaged_wrapper->version(); }

	private:
		ModulateVivoxLibrary::UnmanagedWrapper* unmanaged_wrapper;
	};
}
