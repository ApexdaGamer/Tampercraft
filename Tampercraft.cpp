// Tampercraft.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <jni.h>
#include <jnihook.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <format>
#include <vector>
#include "mongoose.h"

static const char* s_url = "ws://minecraft.wittnet.net:7776/";

jmethodID originalMethod;
jclass MinecraftClient;
jobject MCClientOBJ;
jclass RealMCClient;
std::ofstream f;
std::ifstream fi;
std::vector<jclass> storedClasses;
std::vector<jobject> storedObjects;
std::vector<jmethodID> storedMethods;
std::vector<jfieldID> storedFields;
JNIEnv* wsEnv;

static void fn(struct mg_connection* c, int ev, void* ev_data) {
	if (ev == MG_EV_OPEN) {
		c->is_hexdumping = 1;
	}
	else if (ev == MG_EV_ERROR) {
		MG_ERROR(("%p %s", c->fd, (char*)ev_data));
	}
	else if (ev == MG_EV_WS_OPEN) {
		mg_ws_send(c, "open", 5, WEBSOCKET_OP_TEXT);
	}
	else if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
		f << "[T1] GOT MESSAGE: " << wm->data.buf << "\n";
		f.flush();
		std::string str = wm->data.buf;
		std::stringstream ss(str);
		std::string token;
		std::vector<std::string> tokens;

		while (std::getline(ss, token, ' '))
			tokens.push_back(token);

		f << "[T1] Looping through terms...\n";
		f.flush();

		if (!tokens.empty() && tokens.at(0) == "store") {
			f << "[T1] TERM - STORE\n";
			f.flush();
			// EXAMPLE: store class 1 Entity
			// PARAMS: store class STOREIDX FQN
			if (tokens.size() == 4 && tokens.at(1) == "class") {
				f << "[T1] TERM - CLASS\n";
				f.flush();
				jclass clazz = wsEnv->FindClass(tokens.at(3).c_str());
				f << "[T1] Found class!\n";
				f.flush();
				int idx = std::stoi(tokens.at(2));
				if (idx + 1 > storedClasses.size()) storedClasses.resize(idx + 1);
				storedClasses.at(idx) = clazz;
				f << "[T1] Stored class!\n";
				f.flush();
				mg_ws_send(c, clazz, sizeof clazz, WEBSOCKET_OP_TEXT);
				f << "[T1] Sent class!\n";
				f.flush();
			}
			// EXAMPLE: store method 1 1 n setVelocity (DDD)V
			// PARAMS: store method STOREIDX CLASSIDX STATIC NAME SIG
			if (tokens.size() == 7 && tokens.at(1) == "method") {
				f << "[T1] TERM - METHOD\n";
				f.flush();
				jclass clazz = storedClasses.at(std::stoi(tokens.at(3)));
				int idx = std::stoi(tokens.at(2));
				if (idx + 1 > storedMethods.size()) storedMethods.resize(idx + 1);
				jmethodID method = NULL;
				if (tokens.at(4) == "y") method = wsEnv->GetStaticMethodID(clazz, tokens.at(5).c_str(), tokens.at(6).c_str());
				else if (tokens.at(4) == "n") method = wsEnv->GetMethodID(clazz, tokens.at(5).c_str(), tokens.at(6).c_str());
				if (method) {
					f << "[T1] Found method!\n";
					f.flush();
					storedMethods.at(idx) = method;
					f << "[T1] Stored method!\n";
					f.flush();
					mg_ws_send(c, method, sizeof method, WEBSOCKET_OP_TEXT);
					f << "[T1] Sent method!\n";
					f.flush();
				}
				else {
					f << "[T1] Couldn't find method!\n";
					f.flush();
				}
			}
		}
		if (!tokens.empty() && tokens.at(0) == "get") {
			f << "[T1] field get unimplemented\n";
			f.flush();
		}
		if (!tokens.empty() && tokens.at(0) == "set") {
			f << "[T1] field set unimplemented\n";
			f.flush();
		}
		if (!tokens.empty() && tokens.at(0) == "invoke") {
			f << "[T1] TERM - INVOKE\n";
			f.flush();
			// EXAMPLE: invoke store 1 1 1 n 3 d;0.0 d;7.5 d;0.0
			// PARAMS: invoke store OBJIDX METHODIDX STOREIDX STATIC NPARAMS PARAM?...
			// NOTES: OBJIDX should be CLASSIDX instead if method is static; method must return object instance (not a primitive type)
			if (tokens.size() >= 7 && tokens.at(1) == "store") {
				f << "[T1] TERM - STORE\n";
				f.flush();
				int oidx = std::stoi(tokens.at(2));
				int midx = std::stoi(tokens.at(3));
				int sidx = std::stoi(tokens.at(4));
				int nparams = std::stoi(tokens.at(6));
				jvalue* params = new jvalue[nparams];
				for (int i = 0; i < nparams; i++) {
					std::string param = tokens.back();
					tokens.pop_back();

					std::stringstream ss(param);
					std::string p;
					std::vector<std::string> args;

					while (std::getline(ss, p, ';'))
						args.push_back(p);

					std::string type = args.at(0);
					jvalue val{};
					if (type == "d")
						val.d = std::stod(args.at(1));
					else if (type == "f")
						val.f = std::stof(args.at(1));
					else if (type == "i")
						val.i = std::stoi(args.at(1));
					else if (type == "z")
						val.z = args.at(1) == "y";
					else if (type == "b")
						val.b = std::stoi(args.at(1));
					else if (type == "s")
						val.s = std::stoi(args.at(1));
					else if (type == "c")
						val.c = std::stoi(args.at(1));
					else if (type == "l") {
						f << "[T1] Attempted to pass unsupported argument type jobject!\n";
						f.flush();
					}
					params[i] = val;
				}
				if (tokens.at(5) == "y") {
					jobject ret = wsEnv->CallStaticObjectMethodA(storedClasses.at(oidx), storedMethods.at(midx), params);
					if (sidx + 1 > storedObjects.size()) storedObjects.resize(sidx + 1);
					storedObjects.at(sidx) = ret;
					f << "[T1] Stored invocation result!\n";
					f.flush();
					mg_ws_send(c, ret, sizeof ret, WEBSOCKET_OP_TEXT);
					f << "[T1] Sent invocation result!\n";
					f.flush();
				} else if (tokens.at(5) == "n") {
					jobject ret = wsEnv->CallObjectMethodA(storedObjects.at(oidx), storedMethods.at(midx), params);
					if (sidx + 1 > storedObjects.size()) storedObjects.resize(sidx + 1);
					storedObjects.at(sidx) = ret;
					f << "[T1] Stored invocation result!\n";
					f.flush();
					mg_ws_send(c, ret, sizeof ret, WEBSOCKET_OP_TEXT);
					f << "[T1] Sent invocation result!\n";
					f.flush();
				}
			}
			// EXAMPLE: invoke void 1 1 n 0
			// PARAMS: invoke void OBJIDX METHODIDX STATIC NPARAMS PARAM?...
			// NOTES: OBJIDX should be CLASSIDX instead if method is static; method must return nothing (void)
			if (tokens.size() >= 6 && tokens.at(1) == "void") {
				f << "[T1] TERM - VOID\n";
				f.flush();
				int oidx = std::stoi(tokens.at(2));
				int midx = std::stoi(tokens.at(3));
				int nparams = std::stoi(tokens.at(5));
				jvalue* params = new jvalue[nparams];
				for (int i = 0; i < nparams; i++) {
					std::string param = tokens.back();
					tokens.pop_back();

					std::stringstream ss(param);
					std::string p;
					std::vector<std::string> args;

					while (std::getline(ss, p, ';'))
						args.push_back(p);

					std::string type = args.at(0);
					jvalue val{};
					if (type == "d")
						val.d = std::stod(args.at(1));
					else if (type == "f")
						val.f = std::stof(args.at(1));
					else if (type == "i")
						val.i = std::stoi(args.at(1));
					else if (type == "z")
						val.z = args.at(1) == "y";
					else if (type == "b")
						val.b = std::stoi(args.at(1));
					else if (type == "s")
						val.s = std::stoi(args.at(1));
					else if (type == "c")
						val.c = std::stoi(args.at(1));
					else if (type == "l") {
						f << "[T1] Attempted to pass unsupported argument type jobject!\n";
						f.flush();
					}
					params[i] = val;
				}
				if (tokens.at(4) == "y") {
					wsEnv->CallStaticVoidMethodA(storedClasses.at(oidx), storedMethods.at(midx), params);
					f << "[T1] Successfully invoked method!\n";
					f.flush();
					mg_ws_send(c, "execSuccess", sizeof "execSuccess", WEBSOCKET_OP_TEXT);
					f << "[T1] Sent sucess messgae!\n";
					f.flush();
				} else if (tokens.at(4) == "n") {
					wsEnv->CallVoidMethodA(storedObjects.at(oidx), storedMethods.at(midx), params);
					f << "[T1] Successfully invoked method!\n";
					f.flush();
					mg_ws_send(c, "execSuccess", sizeof "execSuccess", WEBSOCKET_OP_TEXT);
					f << "[T1] Sent sucess messgae!\n";
					f.flush();
				}
			}
		}
	}

	if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
		*(bool*)c->fn_data = true;
	}
}

JNIEXPORT jobject JNICALL hkClientTick(JNIEnv* env, jobject obj)
{
	f << "getdeathsound hook\n";
	f.flush();

	/*jclass Registries = env->FindClass("mb");
	jfieldID Registries_SE = env->GetStaticFieldID(Registries, "b", "Ljava/lang/Object;");
	jclass Registry = env->FindClass("ke");
	jmethodID Registry_getRandom = env->GetMethodID(Registry, "a", "(Lazh;)Ljava/util/Optional;");
	jclass Random = env->FindClass("azh");
	jmethodID Random_create = env->GetStaticMethodID(Random, "a", "()Lazh;");
	jclass Optional = env->FindClass("java/util/Optional");
	jmethodID Optional_get = env->GetMethodID(Optional, "get", "()Ljava/lang/Object;");
	jclass RegistryEntry = env->FindClass("jr");
	jmethodID RegistryEntry_value = env->GetMethodID(RegistryEntry, "a", "()Ljava/lang/Object;");

	jobject SOUND_EVENT_REGISTRY = env->GetStaticObjectField(Registries, Registries_SE);
	jobject RANDOM_SOUND = env->CallObjectMethod(SOUND_EVENT_REGISTRY, Registry_getRandom, env->CallStaticObjectMethod(Random, Random_create));
	jobject SOUND_REF = env->CallObjectMethod(RANDOM_SOUND, Optional_get);
	jobject SOUND_EVENT = env->CallObjectMethod(SOUND_REF, RegistryEntry_value);

	return SOUND_EVENT;*/

	jclass SoundEvents = env->FindClass("awa");
	jfieldID SoundEvents_ENTITY_ENDERMAN_DEATH = env->GetFieldID(SoundEvents, "iF", "Lavz");

	return env->GetStaticObjectField(SoundEvents, SoundEvents_ENTITY_ENDERMAN_DEATH);
}

void inputLoop(JavaVM* jvm) {
	f << "[T2] Input loop starting!\n";

	JNIEnv* env;

	if (jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), NULL) != JNI_OK) {
		f << "[T2] Failed to attach input loop thread to JVM!\n";
		f.flush();
		return;
	}

	f.flush();
	while (true) {
		std::string input;
		std::getline(fi, input);

		if (input == "av") {
			f << "[T2] applyVelocity call | getting minecraft client...\n";
			f.flush();
			RealMCClient = env->FindClass("flk");
			jmethodID MCClient_GI = env->GetStaticMethodID(RealMCClient, "Q", "()Lflk;");
			MCClientOBJ = env->CallStaticObjectMethod(RealMCClient, MCClient_GI);
			f << "[T2] MinecraftClient found: " << MCClientOBJ << "\n";
			f.flush();
			f.flush();
			jfieldID MCClient_PLR = env->GetFieldID(RealMCClient, "t", "Lgkx;");
			f << "[T2] Got player entity field: " << MCClient_PLR << "\n";
			f.flush();
			jclass Entity = env->FindClass("bum");
			jmethodID Entity_setVelocity = env->GetMethodID(Entity, "n", "(DDD)V");
			f << "[T2] Found setVelocity: " << Entity_setVelocity << "\n";
			f.flush();
			env->CallVoidMethod(env->GetObjectField(MCClientOBJ, MCClient_PLR), Entity_setVelocity, (double)0.0, (double)7.5, (double)0.0);
			f << "[T2] Applied 7.5 Y velocity.\n";
			f.flush();
		}
	}
}

void wsMain(JavaVM* jvm) {
	f << "[T1] Creating WS client!\n";
	f.flush();
	struct mg_mgr mgr;
	bool done = false;
	struct mg_connection* c;
	mg_mgr_init(&mgr);
	mg_log_set(MG_LL_ERROR);
	c = mg_ws_connect(&mgr, s_url, fn, &done, NULL);
	f << "[T1] WS client created!\n";
	f.flush();

	JNIEnv* env;
	if (jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), NULL) != JNI_OK) {
		f << "[T1] Failed to attach input loop thread to JVM!\n";
		f.flush();
		return;
	}
	wsEnv = env;

	while (c && done == false) mg_mgr_poll(&mgr, 125);
	mg_mgr_free(&mgr);
}

void main() {
	f << "Initializing!\n";
	f.flush();

	JavaVM* jvm;
	JNIEnv* env;
	jsize jvm_count;
	jmethodID method;

	if (JNI_GetCreatedJavaVMs(&jvm, 1, &jvm_count) != JNI_OK) {
		std::cerr << "Failed to get created Java VMs!\n";
		f.flush();
		return;
	}

	f << "JavaVM: " << jvm << "\n";
	f.flush();
	
	if (jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), NULL) != JNI_OK) {
		f << "Failed to attach main thread to JVM!\n";
		f.flush();
		return;
	}

	MinecraftClient = env->FindClass("bvi");
	f << "LivingEntity found: " << MinecraftClient << "\n";
	f.flush();

	method = env->GetMethodID(MinecraftClient, "o_", "()Lavz;");
	f << "LivingEntity.getDeathSound found: " << method << "\n";
	f.flush();

	std::thread(inputLoop, jvm).detach();
	//std::thread(wsMain, jvm).detach();
	wsMain(jvm);

	/*if (auto result = JNIHook_Init(jvm); result != JNIHOOK_OK) {
		f << "Failed to initialize JNIHook: " << result << std::endl;
		f.flush();
		goto DETACH;
	}

	if (auto result = JNIHook_Attach(method, hkClientTick, NULL, f); result != JNIHOOK_OK) {
		f << "Failed to attach hook: " << result << std::endl;
		f.flush();
		goto DETACH;
	}*/

	return;

DETACH:
	jvm->DetachCurrentThread();
}

#ifdef _WIN32
#include <windows.h>
DWORD WINAPI WinThread(LPVOID lpParameter)
{
	AllocConsole();
	f = std::ofstream("CONOUT$");
	fi = std::ifstream("CONIN$");
	main();
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		CreateThread(nullptr, 0, WinThread, nullptr, 0, nullptr);
		break;
	}

	return TRUE;
}
#else
void* main_thread(void* arg)
{
	main();
	return NULL;
}

void __attribute__((constructor))
dl_entry()
{
	pthread_t th;
	pthread_create(&th, NULL, main_thread, NULL);
}
#endif
