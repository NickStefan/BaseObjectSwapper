#include "SwapData.h"

namespace FormSwap
{
	namespace detail
	{
		static Transform::minMax<float> get_min_max(const std::string& a_str)
		{
			constexpr auto get_float = [](const std::string& str) {
				return string::lexical_cast<float>(str);
			};

			if (const auto splitNum = string::split(a_str, R"(/)"); splitNum.size() > 1) {
				return { get_float(splitNum[0]), get_float(splitNum[1]) };
			} else {
				auto num = get_float(splitNum[0]);
				return { num, num };
			}
		}
	}

	Transform::relData<RE::NiPoint3> Transform::get_transform_from_string(const std::string& a_str)
	{
		bool relative = a_str.contains('R');
		minMax<RE::NiPoint3> transformData;

		srell::cmatch match;
		if (srell::regex_search(a_str.c_str(), match, transformRegex)) {
			const auto [minX, maxX] = detail::get_min_max(match[1].str());  //match[0] gets the whole string
			transformData.first.x = minX;
			transformData.second.x = maxX;

			const auto [minY, maxY] = detail::get_min_max(match[2].str());
			transformData.first.y = minY;
			transformData.second.y = maxY;

			const auto [minZ, maxZ] = detail::get_min_max(match[2].str());
			transformData.first.z = minZ;
			transformData.second.z = maxZ;
		}

		return std::make_pair(relative, transformData);
	}

	std::optional<Transform::minMax<float>> Transform::get_scale_from_string(const std::string& a_str)
	{
		srell::cmatch match;
		if (srell::regex_search(a_str.c_str(), match, genericRegex)) {
			return detail::get_min_max(match[1].str());
		}

		return minMax<float>{ 1.0f, 1.0f };
	}

	float Transform::get_random_value(const RandInput& a_input, float a_min, float a_max)
	{
		if (stl::numeric::essentially_equal(a_min, a_max)) {
			return a_min;
		}

		return a_input.trueRandom ? staticRNG.Generate(a_min, a_max) :
		                            SeedRNG(a_input.refSeed).Generate(a_min, a_max);
	}

	RE::NiPoint3 Transform::get_random_value(const RandInput& a_input, const std::pair<RE::NiPoint3, RE::NiPoint3>& a_minMax)
	{
		auto& [min, max] = a_minMax;

		if (min == max) {
			return min;
		}

		return RE::NiPoint3{
			get_random_value(a_input, min.x, max.x),
			get_random_value(a_input, min.y, max.y),
			get_random_value(a_input, min.z, max.z)
		};
	}

	Transform::Transform(const std::string& a_str)
	{
		if (!a_str.empty() && !string::icontains(a_str, "NONE")) {
			const auto transformStrs = string::split(a_str, ",");
			for (auto& transformStr : transformStrs) {
				if (transformStr.contains("pos")) {
					location = get_transform_from_string(transformStr);
				} else if (transformStr.contains("rot")) {
					rotation = get_transform_from_string(transformStr);
				} else if (transformStr.contains("scale")) {
					refScale = get_scale_from_string(transformStr);
				}
			}
		}
	}

	void Transform::SetTransform(RE::TESObjectREFR* a_refr) const
	{
		if (location || rotation || refScale) {
			const RandInput input(useTrueRandom, a_refr->GetFormID());
			if (location) {
				auto& [relative, minMax] = *location;
				if (relative) {
					a_refr->data.location += get_random_value(input, minMax);
				} else {
					a_refr->data.location = get_random_value(input, minMax);
				}
			}
			if (rotation) {
				auto& [relative, minMax] = *rotation;
				if (relative) {
					a_refr->data.angle += get_random_value(input, minMax);
				} else {
					a_refr->data.angle = get_random_value(input, minMax);
				}
			}
			if (refScale) {
				auto& [min, max] = *refScale;
				const auto scale = std::clamp(get_random_value(input, min, max), 0.0f, 1000.0f);
				a_refr->refScale = static_cast<std::uint16_t>(a_refr->refScale * scale);
			}
		}
	}

    bool Transform::IsValid() const
    {
	    return location || rotation || refScale;
    }

    Traits::Traits(const std::string& a_str)
	{
		if (!a_str.empty() && !string::icontains(a_str, "NONE")) {
			if (a_str.contains("chance")) {
				if (a_str.contains("R")) {
					trueRandom = true;
				}
				srell::cmatch match;
				if (srell::regex_search(a_str.c_str(), match, genericRegex)) {
					chance = string::lexical_cast<std::uint32_t>(match[1].str());
				}
			}
		}
	}

	TransformData::TransformData(const Input& a_input) :
		transform(a_input.transformStr),
		traits(a_input.traitsStr),
		record(a_input.record),
		path(a_input.path)
	{
		if (traits.trueRandom) {
			transform.useTrueRandom = true;
		}
	}

	SwapData::SwapData(FormIDOrSet a_id, const Input& a_input) :
		TransformData(a_input),
		formIDSet(std::move(a_id))
	{}

	RE::FormID TransformData::GetFormID(const std::string& a_str)
	{
		if (a_str.contains('~')) {
			RE::TESForm* form = nullptr;
			if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
				const auto formID = string::lexical_cast<RE::FormID>(splitID[0], true);
				const auto& modName = splitID[1];
				if (g_mergeMapperInterface) {
					const auto [mergedModName, mergedFormID] = g_mergeMapperInterface->GetNewFormID(modName.c_str(), formID);
					form = RE::TESDataHandler::GetSingleton()->LookupForm(mergedFormID, mergedModName);
				} else {
					form = RE::TESDataHandler::GetSingleton()->LookupForm(formID, modName);
				}
			}
			if (form) {
				return form->GetFormID();
			}
		}
		if (const auto form = RE::TESForm::LookupByEditorID(a_str)) {
			return form->GetFormID();
		}
		return static_cast<RE::FormID>(0);
	}

	void TransformData::GetTransforms(const std::string& a_path, const std::string& a_str, std::function<void(RE::FormID, TransformData&)> a_func)
	{
		const auto formPair = string::split(a_str, "|");

		if (const auto baseFormID = GetFormID(formPair[0]); baseFormID != 0) {
			const Input input(
				formPair[1],                                        // transform
				formPair.size() > 2 ? formPair[2] : std::string{},  // traits
				a_str,
				a_path);
			TransformData transformData(input);
			a_func(baseFormID, transformData);
		} else {
			logger::error("			failed to process {} (BASE formID not found)", a_str);
		}
	}

    bool TransformData::IsTransformValid(const RE::TESObjectREFR* a_ref) const
    {
		auto seededRNG = SeedRNG(a_ref->GetFormID());

		if (traits.chance != 100) {
			const auto rng = traits.trueRandom ? staticRNG.Generate() :
			                                     seededRNG.Generate();
			if (rng > traits.chance) {
				return false;
			}
		}

		return transform.IsValid();
    }

    FormIDOrSet SwapData::GetSwapFormID(const std::string& a_str)
	{
		if (a_str.contains(",")) {
			FormIDSet set;
			const auto IDStrs = string::split(a_str, ",");
			set.reserve(IDStrs.size());
			for (auto& IDStr : IDStrs) {
				if (auto formID = GetFormID(IDStr); formID != 0) {
					set.emplace(formID);
				} else {
					logger::error("			failed to process {} (SWAP formID not found)", IDStr);
				}
			}
			return set;
		} else {
			return GetFormID(a_str);
		}
	}

	RE::TESBoundObject* SwapData::GetSwapBase(const RE::TESObjectREFR* a_ref) const
	{
		auto seededRNG = SeedRNG(a_ref->GetFormID());

		if (traits.chance != 100) {
			const auto rng = traits.trueRandom ? staticRNG.Generate() :
			                                     seededRNG.Generate();
			if (rng > traits.chance) {
				return nullptr;
			}
		}

		if (const auto formID = std::get_if<RE::FormID>(&formIDSet); formID) {
			return RE::TESForm::LookupByID<RE::TESBoundObject>(*formID);
		} else {  // return random element from set
			auto& set = std::get<FormIDSet>(formIDSet);

			const auto setEnd = std::distance(set.begin(), set.end()) - 1;
			const auto randIt = traits.trueRandom ? staticRNG.Generate<std::int64_t>(0, setEnd) :
			                                        seededRNG.Generate<std::int64_t>(0, setEnd);

			return RE::TESForm::LookupByID<RE::TESBoundObject>(*std::next(set.begin(), randIt));
		}
	}

	void SwapData::GetForms(const std::string& a_path, const std::string& a_str, std::function<void(RE::FormID, SwapData&)> a_func)
	{
		const auto formPair = string::split(a_str, "|");

		if (const auto baseFormID = GetFormID(formPair[0]); baseFormID != 0) {
			if (const auto swapFormID = GetSwapFormID(formPair[1]); !swap_empty(swapFormID)) {
				const Input input(
					formPair.size() > 2 ? formPair[2] : std::string{},  // transform
					formPair.size() > 3 ? formPair[3] : std::string{},  // traits
					a_str,
					a_path);
				SwapData swapData(swapFormID, input);
				a_func(baseFormID, swapData);
			} else {
				logger::error("			failed to process {} (SWAP formID not found)", a_str);
			}
		} else {
			logger::error("			failed to process {} (BASE formID not found)", a_str);
		}
	}
}
