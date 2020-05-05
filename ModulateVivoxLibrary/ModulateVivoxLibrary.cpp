// ModulateVivoxLibrary.cpp : Defines the functions for the static library.
//

#include "modulate/modulate.h"
#include "ModulateVivoxLibrary.h"

#include "ModulateVivoxIntegration.hpp"

using namespace ModulateVivoxLibrary;

UnmanagedWrapper::UnmanagedWrapper(const std::string& log_dir) {
	modulate_start_text_logging_in_directory(log_dir.c_str());
	vivox_app = new ModulateVivoxIntegration(MODULATE_MAX_SEGMENT_SIZE, voice_skin, log_dir.c_str());
}

UnmanagedWrapper::~UnmanagedWrapper() {
	delete vivox_app;
	for (auto it = voice_skin_map.begin(); it != voice_skin_map.end(); it++)
		modulate_voice_skin_destroy(&it->second);
}

unsigned int UnmanagedWrapper::get_number_of_skins() {
	return (unsigned int)voice_skin_map.size();
}

const std::string& UnmanagedWrapper::get_voice_skin_name(int index) {
	return voice_skin_name_vector[index];
}

void UnmanagedWrapper::select_voice_skin(const std::string& name) {
	void* new_voice_skin = voice_skin_map[name];
	if (new_voice_skin != voice_skin) {
		modulate_voice_skin_reset(new_voice_skin);
		voice_skin = new_voice_skin;
	}
	vivox_app->set_voice_skin(voice_skin);
}

int UnmanagedWrapper::create_voice_skin(const std::string& filename) {
	void* new_voice_skin;
	int error_code = modulate_voice_skin_create(MODULATE_MAX_SEGMENT_SIZE, filename.c_str(), &new_voice_skin);
	if (error_code)
		return error_code;
	std::string voice_skin_name = get_name_from_voice_skin(new_voice_skin);
	voice_skin_map[voice_skin_name] = new_voice_skin;
	voice_skin_name_vector.push_back(voice_skin_name);
	return 0;
}

int UnmanagedWrapper::load_api_key_from_file(const std::string& filename) {
	api_key = load_api_key(filename);
	if (api_key.empty())
		return 1;
	return 0;
}

const std::string UnmanagedWrapper::create_auth_message_for_voice_skin(const std::string& voice_skin_name) {
	void* new_voice_skin = voice_skin_map[voice_skin_name];
	char msg[MODULATE_AUTHENTICATION_MESSAGE_LENGTH];
	int error_code = modulate_voice_skin_create_authentication_message(new_voice_skin,
		api_key.c_str(),
		msg,
		sizeof(msg));
	return std::string(msg);
}

int UnmanagedWrapper::check_auth_message_for_voice_skin(const std::string& voice_skin_name, const std::string& auth_msg) {
	void* new_voice_skin = voice_skin_map[voice_skin_name];
	int error_code = modulate_voice_skin_check_authentication_message(new_voice_skin, auth_msg.c_str());
	return error_code;
}

void UnmanagedWrapper::vivox_start_connect() {
	vivox_app->start();
	vivox_app->connect();
};

int UnmanagedWrapper::vivox_check_connected() {
	bool connected = vivox_app->check_connected();
	return connected;
};

void UnmanagedWrapper::vivox_login() {
	vivox_app->login();
};

void UnmanagedWrapper::vivox_stop() {
	vivox_app->stop();
};

int UnmanagedWrapper::vivox_check_logged_in() {
	bool logged_in = vivox_app->check_logged_in();
	return logged_in;
};

void UnmanagedWrapper::vivox_start_realtime_echo() {
	vivox_app->start_realtime_echo();
};

void UnmanagedWrapper::vivox_end_realtime_echo() {
	vivox_app->end_realtime_echo();
};

void UnmanagedWrapper::vivox_add_session(const std::string& channel_name) {
	vivox_app->add_session(channel_name.c_str(), false);
};

void UnmanagedWrapper::vivox_remove_session(const std::string& channel_name) {
	vivox_app->remove_session(channel_name.c_str(), false);
};

void UnmanagedWrapper::set_radio_strength(float value) {
	vivox_app->set_radio_strength(value);
}

void UnmanagedWrapper::set_presence_strength(float value) {
	vivox_app->set_presence_strength(value);
}

void UnmanagedWrapper::set_bass_booster_strength(float value) {
	vivox_app->set_bass_booster_strength(value);
}

void UnmanagedWrapper::set_intimidator_strength(float value) {
	vivox_app->set_intimidator_strength(value);
}

void UnmanagedWrapper::set_helm_strength(float value) {
	vivox_app->set_helm_strength(value);
}

void UnmanagedWrapper::set_vivid_strength(float value) {
	vivox_app->set_vivid_strength(value);
}

unsigned int UnmanagedWrapper::version() {
	return modulate_get_version();
}

std::string get_name_from_voice_skin(void* voice_skin) {
	char voice_skin_name_buffer[MODULATE_SKIN_NAME_MAX_LENGTH];
	int error_code = modulate_voice_skin_get_skin_name(voice_skin, voice_skin_name_buffer);
	return std::string(voice_skin_name_buffer);
}

std::string load_api_key(const std::string& path) {
	char* buffer = 0;
	long length = 0;
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return "";
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	if (length <= 0) {
		fclose(f);
		return "";
	}
	fseek(f, 0, SEEK_SET);
	buffer = (char*)malloc((length + 1) * sizeof(char));
	if (!buffer) {
		fclose(f);
		return "";
	}
	fread(buffer, sizeof(char), length, f);
	fclose(f);
	buffer[length] = '\0';
	std::string ret(buffer);
	free(buffer);
	ret.erase(std::remove(ret.begin(), ret.end(), '\n'), ret.end());
	ret.erase(std::remove(ret.begin(), ret.end(), '\r'), ret.end());
	return ret;
}