#include "pch.h"

#include "ModulateVivoxWrapper.h"

using namespace ModulateVivoxWrapper;

std::string ModulateVivoxWrapper::undo_windows_system_string(String^ windows_string) {
	std::string ret = msclr::interop::marshal_as<std::string>(windows_string);
	return ret;
}

String^ ModulateVivoxWrapper::create_windows_system_string(const std::string& sane_string) {
	return gcnew String(sane_string.c_str());
}

void ModulateVivoxWrapper::debug(const std::string& str) {
	Console::WriteLine(create_windows_system_string(str));
}