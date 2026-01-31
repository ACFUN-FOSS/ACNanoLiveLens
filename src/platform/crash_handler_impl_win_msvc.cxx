#include "crash_handler.hxx"
#include <iostream>
#include <print>
#include <string>
#include <algorithm>
#include <iterator>
#include <vector>
#include <print>
#include <boost/stacktrace.hpp>
#include "EmergUI/emerg_ui.hxx"
#include "EmergUI/crash_dlg.hxx"


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

// Backtrace code code based on: https://stackoverflow.com/questions/6205981/windows-c-stack-trace-from-a-running-app

// Some versions of imagehlp.dll lack the proper packing directives themselves
// so we need to do it.

//NOLINTBEGIN
#pragma pack(push, before_imagehlp, 8)
#include <format>
#include <imagehlp.h>
#include <sstream>
#pragma pack(pop, before_imagehlp)

struct module_data {
	std::string image_name;
	std::string module_name;
	void* base_address;
	DWORD load_size;
};

class symbol {
	typedef IMAGEHLP_SYMBOL64 sym_type;
	sym_type* sym;
	static const int max_name_len = 1024;

public:
	symbol(HANDLE process, DWORD64 address) :

		sym((sym_type*)::operator new(sizeof(*sym) + max_name_len)) {
		memset(sym, '\0', sizeof(*sym) + max_name_len);
		sym->SizeOfStruct = sizeof(*sym);
		sym->MaxNameLength = max_name_len;
		DWORD64 displacement;

		SymGetSymFromAddr64(process, address, &displacement, sym);
	}

	std::string name() { return std::string(sym->Name); }
	std::string undecorated_name() {
		if (*sym->Name == '\0')
			return "<couldn't map PC to fn name>";
		std::vector<char> und_name(max_name_len);
		UnDecorateSymbolName(sym->Name, &und_name[0], max_name_len, UNDNAME_COMPLETE);
		return std::string(&und_name[0], strlen(&und_name[0]));
	}
};

class get_mod_info {
	HANDLE process;

public:
	get_mod_info(HANDLE h) :
		process(h) {}

	module_data operator()(HMODULE module) {
		module_data ret;
		char temp[4096];
		MODULEINFO mi;

		GetModuleInformation(process, module, &mi, sizeof(mi));
		ret.base_address = mi.lpBaseOfDll;
		ret.load_size = mi.SizeOfImage;

		GetModuleFileNameEx(process, module, temp, sizeof(temp));
		ret.image_name = temp;
		GetModuleBaseName(process, module, temp, sizeof(temp));
		ret.module_name = temp;
		std::vector<char> img(ret.image_name.begin(), ret.image_name.end());
		std::vector<char> mod(ret.module_name.begin(), ret.module_name.end());
		SymLoadModule64(process, 0, &img[0], &mod[0], (DWORD64)ret.base_address, ret.load_size);
		return ret;
	}
};

LONG WINAPI CrashHandlerException(EXCEPTION_POINTERS* ep) {
	HANDLE process = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();
	DWORD offset_from_symbol = 0;
	IMAGEHLP_LINE64 line = { 0 };
	std::vector<module_data> modules;
	DWORD cbNeeded;
	std::vector<HMODULE> module_handles(1);
	std::ostringstream crashInfo;

	if (IsDebuggerPresent()) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	std::cerr << "Program crashed!\n";

	if (assertFailedHandlerTriggered) {
		std::cerr << "跳过崩溃处理，因为之前发生了断言失败" << std::endl;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	switch (ep->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION: {
		DWORD64 targetAddr = ep->ExceptionRecord->ExceptionInformation[1];
		auto action = [ep](){
			switch (ep->ExceptionRecord->ExceptionInformation[0]) {
			case 0:
				return "读取";
			case 1:
				return "写入";
			case 8:
				return "调用";
			default:
				return "未知";
			}
		}();

		crashInfo << std::hex << std::uppercase 
			<< "位于 0x" << targetAddr << " 的内存。该内存不能被"
			<< action << "。\n"
			<< std::dec;
		break;
	}
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_OVERFLOW:


	default: 
		crashInfo << "未知异常" << "\n";
	}

	// Load the symbols:
	if (!SymInitialize(process, nullptr, false))
		return EXCEPTION_CONTINUE_SEARCH;

	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	EnumProcessModules(process, &module_handles[0], (DWORD)module_handles.size() * sizeof(HMODULE), &cbNeeded);
	module_handles.resize(cbNeeded / sizeof(HMODULE));
	EnumProcessModules(process, &module_handles[0], (DWORD)module_handles.size() * sizeof(HMODULE), &cbNeeded);
	std::ranges::transform(module_handles, std::back_inserter(modules), get_mod_info(process));
	void* base = modules[0].base_address;

	// Setup stuff:
	CONTEXT* context = ep->ContextRecord;
	STACKFRAME64 frame;
	bool skip_first = false;

	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;

#ifdef _M_X64
	frame.AddrPC.Offset = context->Rip;
	frame.AddrStack.Offset = context->Rsp;
	frame.AddrFrame.Offset = context->Rbp;
	crashInfo
		<< std::hex << std::uppercase
		<< "rip = 0x" << context->Rip << "\n"
		<< "rsp = 0x" << context->Rsp << "\n"
		<< "rbp = 0x" << context->Rbp << "\n\n"
		<< std::dec;
#else
	frame.AddrPC.Offset = context->Eip;
	frame.AddrStack.Offset = context->Esp;
	frame.AddrFrame.Offset = context->Ebp;

	// Skip the first one to avoid a duplicate on 32-bit mode
	skip_first = true;
#endif

	line.SizeOfStruct = sizeof(line);
	IMAGE_NT_HEADERS* h = ImageNtHeader(base);
	DWORD image_type = h->FileHeader.Machine;


	//fprintf(stderr, "Dumping the backtrace.\n");

	crashInfo << "-- BEGINNING OF BACKTRACE --\n";
	int n = 0;
	do {
		if (skip_first) {
			skip_first = false;
		} else {
			if (frame.AddrPC.Offset != 0) {
				std::string fnName = symbol(process, frame.AddrPC.Offset).undecorated_name();

				if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &offset_from_symbol, &line))
					//fprintf(stderr, "[%d] %s (%s:%d)\n", n, fnName.c_str(), line.FileName, line.LineNumber);
					crashInfo << std::format("#{} {} ({}:{})\n", n, fnName, line.FileName, line.LineNumber);
				else
					//fprintf(stderr, "[%d] %s\n", n, fnName.c_str());
					crashInfo << std::format("#{} {}\n", n, fnName);
			} else
				//fprintf(stderr, "[%d] ???\n", n);
				crashInfo << std::format("#{} ???\n", n);

			n++;
		}

		if (!StackWalk64(image_type, process, hThread, &frame, context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
			break;
	} while (frame.AddrReturn.Offset != 0 && n < 256);

	crashInfo << "-- END OF BACKTRACE --\n";
	SymCleanup(process);

	//crashDlg(CrashReason::ILLEGAL_OP, crashInfo.str());
	spawnCrashDlg({
		.crashReason = CrashReason::ILLEGAL_OP,
		.msg = crashInfo.str()
	});

	// Pass the exception to the OS
	//return EXCEPTION_CONTINUE_SEARCH;

	return EXCEPTION_EXECUTE_HANDLER;
}

// NOLINTEND

int CrtReportHook(int reportType, char* message, int* returnValue) {
   int   nRet = FALSE;

   std::cout << "CRT report hook.\n";
   std::cout << "CRT report type is: ";
   switch   (reportType)
   {
      case _CRT_WARN:
      {
         std::cout << "_CRT_WARN.\n";
         break;
      }


      case _CRT_ERROR:
      {
         std::cout << "_CRT_ERROR.\n";
		 if (IsDebuggerPresent()) {
			 nRet = FALSE;	// Let the debugger handle it
			 break;
		 }

		 nRet = TRUE;   // Always stop for this type of report
		 boost::stacktrace::stacktrace stacktrace;
		 std::stringstream ss;
		 ss << stacktrace;
		 spawnCrashDlg({
			.crashReason = CrashReason::ASSERT_FAIL,
			.msg = ss.str()
		});
		 assertFailedHandlerTriggered = true;
         break;
      }

      case _CRT_ASSERT:
      {
         std::cout << "_CRT_ASSERT.\n";
         //nRet = TRUE;   // Always stop for this type of report
         break;
      }

      default:
      {
         std::cout << "???Unknown???\n";
         break;
      }
   }

   std::println("CRT report message is: {}", message);

   if (nRet && returnValue)
      *returnValue = 1;
   // printf("CRT report code is %d.\n", *pnRet);
   return   nRet;
}

void runProtectedMain() {
	SetUnhandledExceptionFilter(CrashHandlerException);
	_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, CrtReportHook);

	crashHandlerProtectedMain();
}
