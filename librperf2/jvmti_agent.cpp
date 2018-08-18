#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>

#include <iostream>
#include <mutex>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <sstream>

#include "pt_parser.hpp"

template <typename T>
struct jvmti_ptr {
    jvmti_ptr(jvmtiEnv* e, T p): _env(e), _ptr(p) { }
    jvmti_ptr(jvmtiEnv* e): _env(e), _ptr(nullptr) { }
    ~jvmti_ptr() {
	_env->Deallocate((unsigned char*)_ptr);
    }
    jvmti_ptr(const jvmti_ptr&) = delete;
    void operator = (const jvmti_ptr&) = delete;
    T& get() {
	return _ptr;
    }
private:
    jvmtiEnv* _env;
    T _ptr;
};

template <typename T>
jvmti_ptr<T>&& ensure_deallocate(jvmtiEnv* e, T t) {
    return jvmti_ptr<T>(e, t);
}

template <typename T>
jvmti_ptr<T>&& ensure_deallocate(jvmtiEnv* e) {
    return jvmti_ptr<T>(e, nullptr);
}

std::string get_inline_signature(jvmtiEnv* jvmti, jmethodID method);

static void JNICALL
cbMethodCompiled(
    jvmtiEnv *jvmti,
    jmethodID method,
    jint code_size,
    const void* code_addr,
    jint map_length,
    const jvmtiAddrLocationMap* map,
    const void* compile_info)
{

    bool process_method = false;
    
    jvmti_ptr<char*> method_name(jvmti);
    char* sig = nullptr;
    if (!jvmti->GetMethodName(method, &method_name.get(), &sig, NULL)) {
	jclass clazz;
	jvmti_ptr<char*> clsig(jvmti);
	if (!jvmti->GetMethodDeclaringClass(method, &clazz)) {
	    if (!jvmti->GetClassSignature(clazz, &clsig.get(), nullptr)) {
		std::string full_method_name;
		full_method_name += clsig.get();
		full_method_name += "->";
		full_method_name += method_name.get();
		full_method_name += sig;

		//std::cout << "Method: " << full_method_name << std::endl;
		//std::cout << "addr: " << (uint64_t)code_addr << "@" << code_size << std::endl;
		
		//add_symbol(full_method_name, (uint64_t)code_addr, code_size);

		
		const jvmtiCompiledMethodLoadRecordHeader* comp_info =
		    reinterpret_cast<const jvmtiCompiledMethodLoadRecordHeader*>(compile_info);

		if (comp_info->kind == JVMTI_CMLR_INLINE_INFO) {
		    const jvmtiCompiledMethodLoadInlineRecord* inline_info =
			reinterpret_cast<const jvmtiCompiledMethodLoadInlineRecord*>(compile_info);

		    get_symbols_resolver()->add_symbol("java-jit", full_method_name, (uint64_t)code_addr, (uint64_t)inline_info->pcinfo[0].pc - (uint64_t)code_addr);

		    for (int i = 0; i < inline_info->numpcs; ++i) {
			PCStackInfo* info = &inline_info->pcinfo[i];

			std::string inlined_method;


			//std::cout << "inline info: " << std::endl;

			std::string inline_stack;

			for (int i = info->numstackframes - 1; i >= 0; --i) {
			    jmethodID inline_method = info->methods[i];

			    /*
			    jvmti_ptr<char*> inline_method_name(jvmti);
			    jvmti_ptr<char*> inline_method_sign(jvmti);
			    jvmti->GetMethodName(inline_method, &inline_method_name.get(), &inline_method_sign.get(), nullptr);

			    jclass inline_clazz;
			    jvmti->GetMethodDeclaringClass(inline_method, &inline_clazz);
			    jvmti_ptr<char*> inline_class_sign(jvmti);
			    jvmti->GetClassSignature(inline_clazz, &inline_class_sign.get(), nullptr);

			    if (inline_class_sign.get() == nullptr || inline_method_name.get() == nullptr) {
				std::cout << "NULL???" << std::endl;
				::exit(4);
				}
			    */

			    auto method_signature = get_inline_signature(jvmti, inline_method);
			    //std::cout << "\t" << method_signature << std::endl;
			    
			    inline_stack += method_signature;
			    if (i != 0) {
				inline_stack += ">>";
			    }
			}

			//std::cout << "--------- " << inline_stack << std::endl;
			//std::cout << "addr: " << std::hex << (uint64_t)info->pc << std::endl;

			uint64_t iaddr = (uint64_t)info->pc;			
			if (i != (inline_info->numpcs - 1)) {
			    uint64_t inext_addr = (uint64_t)inline_info->pcinfo[i+1].pc;
			    get_symbols_resolver()->add_symbol("java-jit", inline_stack, iaddr, inext_addr - iaddr);
			} else {

			    uint64_t inext_addr = (uint64_t)code_addr + code_size;
			    get_symbols_resolver()->add_symbol("java-jit", inline_stack, iaddr, inext_addr - iaddr);
			}

		    }
		}
	    } else {
		get_symbols_resolver()->add_symbol("java-jit", "unknown1", (uint64_t)code_addr, code_size);
	    }
	} else {
	    get_symbols_resolver()->add_symbol("java-jit", "unknown2", (uint64_t)code_addr, code_size);
	}
    } else {
      get_symbols_resolver()->add_symbol("java-jit", "unknown3", (uint64_t)code_addr, code_size);
    }
}

std::string get_inline_signature(jvmtiEnv* jvmti, jmethodID method) {
    std::stringstream result;

    jvmti_ptr<char*> method_name(jvmti);
    jvmti_ptr<char*> method_sign(jvmti);
    if (!jvmti->GetMethodName(method, &method_name.get(), &method_sign.get(), nullptr)) {
	jclass clazz;
	if (!jvmti->GetMethodDeclaringClass(method, &clazz)) {
	    jvmti_ptr<char*> class_sign(jvmti);
	    if (!jvmti->GetClassSignature(clazz, &class_sign.get(), NULL)) {
		
		//result << class_sign.get() << "::" << method_name.get() << " / " << method_sign.get();
		result << class_sign.get() << method_name.get();

		jvmti_ptr<char*> source_file(jvmti);
		if (!jvmti->GetSourceFileName(clazz, &source_file.get())) {
		    jint entry_count;
		    jvmti_ptr<jvmtiLineNumberEntry*> lines(jvmti);
		    if (!jvmti->GetLineNumberTable(method, &entry_count, &lines.get())) {
			if (entry_count > 0) {
			    result << "(" << source_file.get() << ":" << lines.get()[0].line_number << ")";
			}
		    }
		}
	    }
	}
    }
    
    return result.str();
}

void JNICALL
cbMethodDynamic(jvmtiEnv *jvmti,
            const char* name,
            const void* address,
            jint length)
{
    //std::cout << "dynamic method" << name << "@" << address << ":" << length << std::endl;
    get_symbols_resolver()->add_symbol("java-dyn", name, (uint64_t)address, length);
}

void JNICALL cbMethodUnload
    (jvmtiEnv *jvmti_env,
     jmethodID method,
     const void* code_addr) {
    std::cout << "Unload: " << method << "@" << std::hex << code_addr << std::endl;
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    std::cout << "Hello world from JVMTI" << std::endl;

    jvmtiEnv* jvmti = nullptr;
    {
	auto status = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1);
    }

    {
	jvmtiCapabilities caps = {};

	caps.can_generate_all_class_hook_events  = 1;
	caps.can_tag_objects                     = 1;
	caps.can_generate_object_free_events     = 1;
	caps.can_get_source_file_name            = 1;
	caps.can_get_line_numbers                = 1;
	caps.can_generate_vm_object_alloc_events = 1;
	caps.can_generate_compiled_method_load_events = 1;

	auto status = jvmti->AddCapabilities(&caps);
    }

    {
	jvmtiEventCallbacks callbacks = {};

	callbacks.CompiledMethodLoad = &cbMethodCompiled;
	callbacks.DynamicCodeGenerated = &cbMethodDynamic;
	callbacks.CompiledMethodUnload = &cbMethodUnload;

	auto status = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    }

    {
 	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, (jthread)NULL);
	jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
    }

    {
	auto status = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
	status = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_UNLOAD);
	status = jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
    }

    return 0;
}
