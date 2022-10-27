#pragma once

#define WIN32_LEAN_AND_MEAN

#undef GetForm

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include "XoshiroCpp.hpp"
#include <MergeMapperPluginAPI.h>
#include <SimpleIni.h>
#include <ranges>
#include <robin_hood.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <srell.hpp>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
namespace string = SKSE::stl::string;

using namespace std::literals;

namespace stl
{
	using namespace SKSE::stl;

	template <class F, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		T::func = vtbl.write_vfunc(T::size, T::thunk);
	}
}

#include "Version.h"
