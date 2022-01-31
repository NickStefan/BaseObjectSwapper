#include "FormSwapManager.h"
#include "MergeMapper.h"

namespace FormSwap
{
	struct InitItemImpl
	{
		static void thunk(RE::TESObjectREFR* a_ref)
		{
			const auto base = a_ref->GetBaseObject();

			if (base) {
				FormSwapManager::GetSingleton()->LoadFormsOnce();

			    auto [swapBase, flags] = FormSwapManager::GetSingleton()->GetSwapData(a_ref, base);

				if (swapBase && base != swapBase) {
				    a_ref->SetObjectReference(swapBase);

					const FormData originalData = { base->GetFormID(), flags };
					FormSwapManager::GetSingleton()->SetOriginalBase(a_ref, originalData);
				}
			}

			func(a_ref);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t size = 0x13;
	};

	struct Load3D
	{
		static RE::NiAVObject* thunk(RE::TESObjectREFR* a_ref, bool a_backgroundLoading)
		{
			const auto node = func(a_ref, a_backgroundLoading);

			if (!a_ref->IsDynamicForm() && node) {
				auto [origBase, flags] = FormSwapManager::GetSingleton()->GetOriginalBase(a_ref);

				if (origBase && flags.all(SWAP_FLAGS::kApplyMaterialShader)) {
					const auto stat = origBase->As<RE::TESObjectSTAT>();
					if (const auto shader = stat ? stat->data.materialObj : nullptr; shader) {
						const auto projectedUVParams = RE::NiColorA{
							shader->directionalData.falloffScale,
							shader->directionalData.falloffBias,
							1.0f / shader->directionalData.noiseUVScale,
							std::cosf(RE::deg_to_rad(stat->data.materialThresholdAngle))
						};

						node->SetProjectedUVData(
							projectedUVParams,
							shader->directionalData.singlePassColor,
							shader->directionalData.flags.any(RE::BSMaterialObject::DIRECTIONAL_DATA::Flag::kSnow));
					}
				}
			}

			return node;
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static inline constexpr std::size_t size = 0x6A;
	};

	inline void Install()
	{
		stl::write_vfunc<RE::TESObjectREFR, Load3D>();
		stl::write_vfunc<RE::TESObjectREFR, InitItemImpl>();

	    logger::info("Installed form swap"sv);
	}
}

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
		FormSwapManager::GetSingleton()->PrintConflicts();
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "Base Object Swapper";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver <
#ifndef SKYRIMVR
		SKSE::RUNTIME_1_5_39
#else
		SKSE::RUNTIME_VR_1_4_15
#endif
	) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded plugin");

	SKSE::Init(a_skse);

	logger::info("{:*^30}", "MERGES");
	MergeMapper::GetMerges();

	logger::info("{:*^30}", "HOOKS");

	FormSwap::Install();

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
