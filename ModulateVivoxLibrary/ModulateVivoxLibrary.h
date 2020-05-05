#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <string>
#include <map>
#include <vector>
#include <algorithm>

#define MODULATE_MAX_SEGMENT_SIZE 2400

std::string get_name_from_voice_skin(void* voice_skin);
std::string load_api_key(const std::string& path);

class ModulateVivoxIntegration;

namespace ModulateVivoxLibrary {
	class UnmanagedWrapper {
	public:
		UnmanagedWrapper(const std::string& _log_dir);
		~UnmanagedWrapper();

		unsigned int get_number_of_skins();
		const std::string& get_voice_skin_name(int index);
		void select_voice_skin(const std::string& skin_name);

		int create_voice_skin(const std::string& _filename);
		int load_api_key_from_file(const std::string& _filename);
		const std::string create_auth_message_for_voice_skin(const std::string& _voice_skin_name);
		int check_auth_message_for_voice_skin(const std::string& _voice_skin_name, const std::string& _auth_msg);

		void vivox_start_connect();
		int vivox_check_connected();
		void vivox_login();
		void vivox_stop();
		int vivox_check_logged_in();
		void vivox_start_realtime_echo();
		void vivox_end_realtime_echo();
		void vivox_add_session(const std::string& _channel_name);
		void vivox_remove_session(const std::string& _channel_name);

		void set_radio_strength(float value);
		void set_presence_strength(float value);
		void set_bass_booster_strength(float value);
		void set_intimidator_strength(float value);
		void set_helm_strength(float value);
		void set_vivid_strength(float value);

		unsigned int version();

	private:
		void* voice_skin = nullptr;
		std::map < const std::string, void*> voice_skin_map;
		std::vector<std::string> voice_skin_name_vector;
		std::string api_key;

		ModulateVivoxIntegration* vivox_app;
	};
}