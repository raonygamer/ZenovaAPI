#pragma once

#include <string>
#include <unordered_map>
#include <tuple>
#include <vector> //std::vector
#include <utility> //std::pair
#include <type_traits>
#include <iostream>

#include "Common.h"
#include "Log.h"
#include "Platform.h"

namespace Zenova {
	//Sorry ogniK :P
	enum class CallConvention { CDecl, ClrCall, StdCall, FastCall, ThisCall, VectorCall };
	template <CallConvention C, typename T, typename... ARGS>
	struct BuildCallable {
		constexpr static auto value() {
			if constexpr(C == CallConvention::CDecl) {
				return static_cast<T(_cdecl*)(ARGS...)>(nullptr);
			}
			else if constexpr(C == CallConvention::ClrCall) {
			#ifndef __cplusplus_cli
				static_assert(false, "Requires /clr or /ZW command line option");
			#else
				return static_cast<T(__clrcall*)(ARGS...)>(nullptr);
			#endif
			}
			else if constexpr(C == CallConvention::StdCall) {
				return static_cast<T(__stdcall*)(ARGS...)>(nullptr);
			}
			else if constexpr(C == CallConvention::FastCall) {
				return static_cast<T(__fastcall*)(ARGS...)>(nullptr);
			}
			else if constexpr(C == CallConvention::ThisCall) {
				return static_cast<T(__thiscall*)(ARGS...)>(nullptr);
			}
			else if constexpr(C == CallConvention::VectorCall) {
				return static_cast<T(__vectorcall*)(ARGS...)>(nullptr);
			}
			else {
				static_assert(false, "Invalid call convention");
			}
		}
	};

	namespace Hook {
		extern std::unordered_map<std::string, uintptr_t> functions;
		extern std::unordered_map<std::string, uintptr_t> vtables;
		extern std::unordered_map<std::string, uintptr_t> variables;
		//extern std::unordered_map<std::string, std::unordered_map<std::string, uintptr_t>> Symbols;

		EXPORT uintptr_t SlideAddress(std::size_t offset);
		EXPORT std::size_t UnslideAddress(uintptr_t result);
		EXPORT uintptr_t FindMangledSymbol(const char* function);
		EXPORT uintptr_t FindVTable(const char* vtable);
		EXPORT uintptr_t FindVariable(const char* variable);

		// virtualDtor = address of the generated dtor in the vtable (msvc)
		EXPORT uintptr_t GetRealDtor(uintptr_t virtualDtor);
		EXPORT uintptr_t Sigscan(const char* sig, const char* mask);
		EXPORT uintptr_t SigscanCall(const char* sig, const char* mask);

		EXPORT bool Create(void* function, void* funcJump, void* funcTrampoline);
		EXPORT bool Create(void* vtable, void* function, void* funcJump, void* funcTrampoline);

		template <typename T,
			std::enable_if_t<std::is_function<typename std::remove_pointer<T>::type>::value>* = nullptr>
		bool Create(T function, void* funcJump, void* funcTrampoline) {
			return Create(*reinterpret_cast<void**>(&function), funcJump, funcTrampoline);
		}

		template <typename T,
			std::enable_if_t<std::is_member_function_pointer<typename std::remove_pointer<T>::type>::value>* = nullptr>
		bool Create(T function, void* funcJump, void* funcTrampoline) {
			return Create(*reinterpret_cast<void**>(&function), funcJump, funcTrampoline);
		}

		template <typename T,
			std::enable_if_t<std::is_member_function_pointer<typename std::remove_pointer<T>::type>::value>* = nullptr>
		bool Create(void* vtable, T function, void* funcJump, void* funcTrampoline) {
			return Create(vtable, *reinterpret_cast<void**>(&function), funcJump, funcTrampoline);
		}

		template<typename T, CallConvention C, typename... Targs>
		T Call(uintptr_t func, Targs... args) {
			using CALLABLE = decltype(BuildCallable<C, T, Targs...>::value());
			
			if constexpr (std::is_void_v<T>) {
				reinterpret_cast<CALLABLE>(func)(args...);
			} else {
				return reinterpret_cast<CALLABLE>(func)(args...);
			}
		}

		template<typename T, CallConvention C = CallConvention::CDecl, typename... Targs>
		T Call(const char* func, Targs... args) {
			if constexpr (std::is_void_v<T>) {
				Call<T, C>(FindMangledSymbol(func), args...);
			} else {
				return Call<T, C>(FindMangledSymbol(func), args...);
			}
		}
	};
}

// defines the cast to choose an overloaded member function
// ex: Zenova_OCast(void, Test, int)(&Test::overload); chooses void Test::overload(int)
#define Zenova_OCast(r, c, ...) static_cast<r (c::*)(__VA_ARGS__)>

// ex: Zenova_Hook(Test::func, &func, &_func);
#define Zenova_Hook(function, hook, trampoline) do { \
	if (!Zenova::Hook::Create(&function, hook, trampoline)) { \
		Zenova_Info(#function " hook failed"); \
	} \
} while (0)

// ex: Zenova_VHook(Test, vfunc, &func, &_func); hooks Test::Vfunc
#define Zenova_VHook(classname, function, hook, trampoline, ...) do { \
	if (!Zenova::Hook::Create(classname##_vtable, (__VA_ARGS__(&classname::function)), hook, trampoline)) { \
		Zenova_Info(#classname "::" #function " vhook failed"); \
	} \
} while (0)