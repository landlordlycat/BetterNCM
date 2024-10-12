#pragma once
#include "pch.h"
#include "EasyCEFHooks.h"
#include "3rd/libcef/include/capi/cef_base_capi.h"
#include "utils/Interprocess.hpp"

class LambdaTask {
	using cef_task_post_exec = struct _cef_task_post_exec {
		cef_task_t task;
		std::function<void()> lambda;
	};

	static void CEF_CALLBACK exec(struct _cef_task_t* self) {
		auto task = (cef_task_post_exec*)self;
		task->lambda();
	}

	cef_task_runner_t* runner;

public:
	LambdaTask(cef_task_runner_t* runner) {
		this->runner = runner;
		this->runner->base.add_ref(&this->runner->base);
	}

	~LambdaTask() {
	}

	template<typename F>
	void post(F&& f) const {
		auto task = static_cast<cef_task_post_exec*>(calloc(1, sizeof(cef_task_post_exec)));
		task->lambda = f;
		task->task.base.size = sizeof(cef_task_t);
		task->task.execute = exec;
		this->runner->post_task(runner, (cef_task_t*)task);
	}
};

_cef_frame_t* frame = nullptr;
cef_v8context_t* contextl = nullptr;

struct _cef_client_t* cef_client = nullptr;
PVOID origin_cef_browser_host_create_browser = nullptr;
PVOID origin_cef_initialize = nullptr;
PVOID origin_cef_execute_process = nullptr;
PVOID origin_cef_get_keyboard_handler = nullptr;
PVOID origin_cef_client_get_display_handler = nullptr;
PVOID origin_cef_on_key_event = nullptr;
PVOID origin_cef_v8context_get_current_context = nullptr;
PVOID origin_cef_load_handler = nullptr;
PVOID origin_cef_on_load_start = nullptr;
PVOID origin_cef_app_on_context_created = nullptr;
PVOID origin_cef_app_on_context_released = nullptr;
PVOID origin_on_before_command_line_processing = nullptr;
PVOID origin_get_render_process_handler = nullptr;
PVOID origin_command_line_append_switch = nullptr;
PVOID origin_cef_register_scheme_handler_factory = nullptr;
PVOID origin_cef_scheme_handler_create = nullptr;
PVOID origin_scheme_handler_read = nullptr;
PVOID origin_get_headers = nullptr;

std::function<void(struct _cef_browser_t* browser, struct _cef_frame_t* frame)> EasyCEFHooks::onLoadStart = [
](auto browser, auto frame) {
};
std::function<void(_cef_client_t*, struct _cef_browser_t*, const struct _cef_key_event_t*)> EasyCEFHooks::onKeyEvent = [
](auto client, auto browser, auto key) {
};
std::function<bool(std::string)> EasyCEFHooks::onAddCommandLine = [](std::string arg) { return true; };
std::function<std::function<std::wstring(std::wstring)>(std::string)> EasyCEFHooks::onHijackRequest =
[](std::string url) { return nullptr; };
std::function<void(struct _cef_command_line_t* command_line)> EasyCEFHooks::onCommandLine = [
](struct _cef_command_line_t* command_line) {
};


int CEF_CALLBACK hook_cef_on_key_event(struct _cef_keyboard_handler_t* self,
	struct _cef_browser_t* browser,
	const struct _cef_key_event_t* event,
	cef_event_handle_t os_event) {
	EasyCEFHooks::onKeyEvent(cef_client, browser, event);

	return CAST_TO(origin_cef_on_key_event, hook_cef_on_key_event)(self, browser, event, os_event);
}


void process_context(cef_v8context_t* context);

// Deprecated
cef_v8context_t* hook_cef_v8context_get_current_context() {
	cef_v8context_t* context = CAST_TO(origin_cef_v8context_get_current_context,
		hook_cef_v8context_get_current_context)();
	return context;
}

struct _cef_keyboard_handler_t* CEF_CALLBACK hook_cef_get_keyboard_handler(struct _cef_client_t* self) {
	auto keyboard_handler = CAST_TO(origin_cef_get_keyboard_handler, hook_cef_get_keyboard_handler)(self);
	if (keyboard_handler) {
		cef_client = self;
		origin_cef_on_key_event = keyboard_handler->on_key_event;
		keyboard_handler->on_key_event = hook_cef_on_key_event;
	}
	return keyboard_handler;
}


void CEF_CALLBACK hook_cef_on_load_start(struct _cef_load_handler_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	cef_transition_type_t transition_type) {
	EasyCEFHooks::onLoadStart(browser, frame);
	CAST_TO(origin_cef_on_load_start, hook_cef_on_load_start)(self, browser, frame, transition_type);
}

void CEF_CALLBACK hook_cef_on_load_error(struct _cef_load_handler_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	cef_errorcode_t errorCode,
	const cef_string_t* errorText,
	const cef_string_t* failedUrl) {
	EasyCEFHooks::executeJavaScript(
		frame,
		R"(if (location.href === "chrome-error://chromewebdata/") location.href = "orpheus://orpheus/pub/app.html")",
		"libeasycef/fix_white_screen.js");
}

struct _cef_load_handler_t* CEF_CALLBACK hook_cef_load_handler(struct _cef_client_t* self) {
	auto load_handler = CAST_TO(origin_cef_load_handler, hook_cef_load_handler)(self);
	if (load_handler) {
		cef_client = self;
		load_handler->on_load_error = hook_cef_on_load_error;
		origin_cef_on_load_start = load_handler->on_load_start;
		load_handler->on_load_start = hook_cef_on_load_start;
	}
	return load_handler;
}


_cef_display_handler_t* CEF_CALLBACK hook_cef_client_get_display_handler(_cef_client_t* self) {
	_cef_display_handler_t* display_handler = CAST_TO(origin_cef_client_get_display_handler,
		hook_cef_client_get_display_handler)(self);
	display_handler->on_title_change = [](struct _cef_display_handler_t* self,
		struct _cef_browser_t* browser,
		const cef_string_t* title) -> void {
			auto frame = browser->get_main_frame(browser);
			if (frame && !BNString(util::cefFromCEFUserFreeTakeOwnership(frame->get_url(frame)).ToWString()).startsWith(
				L"devtools://")) {
				auto host = browser->get_host(browser);
				auto hwnd = host->get_window_handle(host);
				SetWindowText(hwnd, std::wstring(util::cefFromCEFUserFree(title)).c_str());
			}
	};

	return display_handler;
}

cef_browser_t* hook_cef_browser_host_create_browser(
	const cef_window_info_t* windowInfo,
	struct _cef_client_t* client,
	const cef_string_t* url,
	const struct _cef_browser_settings_t* settings,
	struct _cef_dictionary_value_t* extra_info,
	struct _cef_request_context_t* request_context) {
	origin_cef_get_keyboard_handler = client->get_keyboard_handler;
	client->get_keyboard_handler = hook_cef_get_keyboard_handler;

	
	origin_cef_load_handler = client->get_load_handler;
	client->get_load_handler = hook_cef_load_handler;
	 
	origin_cef_client_get_display_handler = client->get_display_handler;
	client->get_display_handler = hook_cef_client_get_display_handler;

	cef_browser_t* origin = CAST_TO(origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser)
		(windowInfo, client, url, settings, extra_info, request_context);
	return origin;
}

void CEF_CALLBACK hook_command_line_append_switch(_cef_command_line_t* self, const cef_string_t* name) {
	if (EasyCEFHooks::onAddCommandLine(util::cefFromCEFUserFree(name).ToString())) {
		CAST_TO(origin_command_line_append_switch, hook_command_line_append_switch)(self, name);
	}
} 
 
void CEF_CALLBACK hook_on_before_command_line_processing(
	struct _cef_app_t* self,
	const cef_string_t* process_type,
	struct _cef_command_line_t* command_line) {
	EasyCEFHooks::onCommandLine(command_line);
	origin_command_line_append_switch = command_line->append_switch;
	command_line->append_switch = hook_command_line_append_switch;
	CAST_TO(origin_on_before_command_line_processing, hook_on_before_command_line_processing)(
		self, process_type, command_line);
}


void CEF_CALLBACK hook_on_context_created(
	struct _cef_render_process_handler_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	struct _cef_v8context_t* context) {
	auto url = BNString(util::cefFromCEFUserFreeTakeOwnership(frame->get_url(frame)).ToWString());
	if (url.startsWith(L"orpheus://")) {
		process_context(context);

		CAST_TO(origin_cef_app_on_context_created, hook_on_context_created)(self, browser, frame, context);
	}
}

void CEF_CALLBACK hook_on_context_released(
	struct _cef_render_process_handler_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	struct _cef_v8context_t* context) {
	if (BNString(util::cefFromCEFUserFreeTakeOwnership(frame->get_url(frame)).ToWString()).startsWith(L"orpheus://"))
		CAST_TO(origin_cef_app_on_context_released, hook_on_context_released)(self, browser, frame, context);
}

struct _cef_render_process_handler_t* CEF_CALLBACK hook_get_render_process_handler(struct _cef_app_t* self) {
	auto handler = CAST_TO(origin_get_render_process_handler, hook_get_render_process_handler)(self);

	origin_cef_app_on_context_created = handler->on_context_created;
	handler->on_context_created = hook_on_context_created;
	origin_cef_app_on_context_released = handler->on_context_released;
	handler->on_context_released = hook_on_context_released;
	handler->on_browser_destroyed = nullptr;
	return handler;
}

int hook_cef_execute_process(const struct _cef_main_args_t* args,
	cef_app_t* application,
	void* windows_sandbox_info) {
	origin_get_render_process_handler = application->get_render_process_handler;
	application->get_render_process_handler = hook_get_render_process_handler;

	return CAST_TO(origin_cef_execute_process, hook_cef_execute_process)(args, application, windows_sandbox_info);
}

int hook_cef_initialize(const struct _cef_main_args_t* args,
	const struct _cef_settings_t* settings,
	cef_app_t* application,
	void* windows_sandbox_info) {
	_cef_settings_t s = *settings;
	s.background_color = 0x000000ff;

	origin_on_before_command_line_processing = application->on_before_command_line_processing;
	application->on_before_command_line_processing = hook_on_before_command_line_processing;

	origin_get_render_process_handler = application->get_render_process_handler;
	application->get_render_process_handler = hook_get_render_process_handler;

	return CAST_TO(origin_cef_initialize, hook_cef_initialize)(args, &s, application, windows_sandbox_info);
}


class CefRequestMITMProcess {
	const static int bytesPerTime = 65535;

public:
	std::string url;
	std::vector<char> data;
	int datasize = 0;
	int dataPointer = 0;

	void fillData(const std::wstring& s) {
		fillData(util::wstring_to_utf8(s));
	};

	void fillData(const std::string& s) {
		data = std::vector<char>(s.begin(), s.end());
	};
	void fillData(_cef_resource_handler_t* self, _cef_callback_t* callback);

	std::wstring getDataStr() {
		try {
			return util::utf8_to_wstring(std::string(data.begin(), data.end()));
		}
		catch (std::exception& e) {
			util::alert(e.what());
			return L"";
		}
	}

	bool dataFilled() {
		return data.size();
	}

	int sendData(void* data_out,
		int bytes_to_read,
		int* bytes_read) {
		int dataSize = min(bytes_to_read, data.size() - dataPointer);

#ifdef DEBUG
		cout << dataSize << " bytes copied\n";
#endif

		if (dataSize == 0) {
			*bytes_read = 0;
			return 0;
		}

		std::copy(std::next(data.begin(), dataPointer),
			std::next(data.begin(), dataPointer + dataSize),
			static_cast<char*>(data_out));

		dataPointer += dataSize;
		*bytes_read = dataSize;

		return 1;
	}
};

std::map<_cef_resource_handler_t*, CefRequestMITMProcess> urlMap;

int CEF_CALLBACK hook_scheme_handler_read(struct _cef_resource_handler_t* self,
	void* data_out,
	int bytes_to_read,
	int* bytes_read,
	struct _cef_callback_t* callback) {
	if (urlMap[self].dataFilled()) {
		return urlMap[self].sendData(data_out, bytes_to_read, bytes_read);
	}

	auto tick = GetTickCount64();
	auto processor = EasyCEFHooks::onHijackRequest(urlMap[self].url);

	if (processor) {
		urlMap[self].fillData(self, callback);
		urlMap[self].fillData(util::wstring_to_utf8(processor(urlMap[self].getDataStr())));

		std::cout << "[ BetterNCM Hijack ]" << urlMap[self].url << " hijacked, time used: " << GetTickCount64() - tick
			<< "ms\n";
		if (urlMap[self].sendData(data_out, bytes_to_read, bytes_read))return 1;
		urlMap.erase(self);
		return 0;
	}
	return CAST_TO(origin_scheme_handler_read, hook_scheme_handler_read)(
		self, data_out, bytes_to_read, bytes_read, callback);
}


_cef_resource_handler_t* CEF_CALLBACK hook_cef_scheme_handler_create(
	struct _cef_scheme_handler_factory_t* self,
	struct _cef_browser_t* browser,
	struct _cef_frame_t* frame,
	const cef_string_t* scheme_name,
	struct _cef_request_t* request) {
	_cef_resource_handler_t* ret = CAST_TO(origin_cef_scheme_handler_create, hook_cef_scheme_handler_create)(
		self, browser, frame, scheme_name, request);
	CefString url = util::cefFromCEFUserFreeTakeOwnership(request->get_url(request));
	urlMap[ret] = CefRequestMITMProcess{
			url.ToString()
	};

	origin_scheme_handler_read = ret->read_response;
	ret->read_response = hook_scheme_handler_read;
	return ret;
}


void CefRequestMITMProcess::fillData(_cef_resource_handler_t* self, _cef_callback_t* callback) {
	if (data.size())return;
	auto bytes_read = new int(0);
	auto outdata = new char[bytesPerTime];
	_cef_callback_t a{};
	while (CAST_TO(origin_scheme_handler_read, hook_scheme_handler_read)
		(self, outdata, bytesPerTime - 1, bytes_read, &a)) {
		data.insert(data.end(), outdata, outdata + (*bytes_read));
		datasize += *bytes_read;

		*bytes_read = 0;
	}
}


int hook_cef_register_scheme_handler_factory(
	const cef_string_t* scheme_name,
	const cef_string_t* domain_name,
	cef_scheme_handler_factory_t* factory) {
	origin_cef_scheme_handler_create = factory->create;
	factory->create = hook_cef_scheme_handler_create;

	int ret = CAST_TO(origin_cef_register_scheme_handler_factory, hook_cef_register_scheme_handler_factory)(
		scheme_name, domain_name, factory);
	return ret;
}


bool EasyCEFHooks::InstallHooks() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());


	origin_cef_v8context_get_current_context = DetourFindFunction("libcef.dll", "cef_v8context_get_current_context");
	origin_cef_browser_host_create_browser = DetourFindFunction("libcef.dll", "cef_browser_host_create_browser_sync");
	origin_cef_initialize = DetourFindFunction("libcef.dll", "cef_initialize");
	origin_cef_execute_process = DetourFindFunction("libcef.dll", "cef_execute_process");
	origin_cef_register_scheme_handler_factory =
		DetourFindFunction("libcef.dll", "cef_register_scheme_handler_factory");


	
	if (origin_cef_v8context_get_current_context)
		DetourAttach(&origin_cef_v8context_get_current_context, hook_cef_v8context_get_current_context);

	if (origin_cef_browser_host_create_browser)
		DetourAttach(&origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser);

	if (origin_cef_register_scheme_handler_factory)
		DetourAttach(&origin_cef_register_scheme_handler_factory, hook_cef_register_scheme_handler_factory);

	if (origin_cef_initialize)
		DetourAttach(&origin_cef_initialize, hook_cef_initialize);

	if (origin_cef_execute_process)
		DetourAttach(&origin_cef_execute_process, hook_cef_execute_process);

	LONG ret = DetourTransactionCommit();

	cef_v8context_get_current_context();
	return ret == NO_ERROR;
}

bool EasyCEFHooks::UninstallHook() {
	//DetourTransactionBegin();
	//DetourUpdateThread(GetCurrentThread());
	//DetourDetach(&origin_cef_browser_host_create_browser, hook_cef_browser_host_create_browser);
	//DetourDetach(&origin_cef_register_scheme_handler_factory, hook_cef_register_scheme_handler_factory);
	//DetourDetach(&origin_cef_initialize, hook_cef_initialize);
	//DetourDetach(&origin_cef_v8context_get_current_context, hook_cef_v8context_get_current_context);

	//LONG ret = DetourTransactionCommit();
	return true;
}

void EasyCEFHooks::executeJavaScript(_cef_frame_t* frame, const std::string& script, const std::string& url) {
	CefString exec_script = script;
	CefString purl = url;
	frame->execute_java_script(frame, exec_script.GetStruct(), purl.GetStruct(), 0);
}
