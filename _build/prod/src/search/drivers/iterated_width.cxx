

#include <lapkt/novelty/features.hxx>
#include <search/drivers/iterated_width.hxx>
#include <search/algorithms/iterated_width.hxx>
#include <search/novelty/fs_novelty.hxx>
#include <search/utils.hxx>
#include <problem_info.hxx>
#include <search/drivers/setups.hxx>

#include <search/drivers/sbfws/base.hxx>
#include <search/drivers/sbfws/features/features.hxx>


namespace fs0 { namespace drivers {


template <typename StateModelT, typename FeatureEvaluatorT, typename NoveltyEvaluatorT>
std::unique_ptr<FS0IWAlgorithm<StateModelT, FeatureEvaluatorT, NoveltyEvaluatorT>>
create(const Config& config, FeatureEvaluatorT&& featureset, const StateModelT& model, SearchStats& stats) {
	using EngineT = FS0IWAlgorithm<StateModelT, FeatureEvaluatorT, NoveltyEvaluatorT>;
	using EnginePT = std::unique_ptr<EngineT>;
	
	unsigned max_novelty = config.getOption<int>("width.max");
	assert(0); // TO REIMPLEMENT
// 	auto evaluator = fs0::bfws::create_novelty_evaluator<NoveltyEvaluatorT>(model.getTask(), fs0::bfws::SBFWSConfig::NoveltyEvaluatorType::Adaptive, max_novelty);
	auto evaluator = nullptr;
	return EnginePT(new EngineT(model, 1, max_novelty, std::move(featureset), evaluator, stats));
}




template <typename StateModelT, typename NoveltyEvaluatorT, typename FeatureEvaluatorT>
ExitCode
do_search1(const StateModelT& model, FeatureEvaluatorT&& featureset, const Config& config, const std::string& out_dir, float start_time, SearchStats& stats) {
	auto engine = create<StateModelT, FeatureEvaluatorT, NoveltyEvaluatorT>(config, std::move(featureset), model, stats);
	return drivers::Utils::do_search(*engine, model, out_dir, start_time, stats);
}




template <typename StateModelT>
ExitCode
do_search(const StateModelT& model, const Config& config, const std::string& out_dir, float start_time, SearchStats& stats) {
	const StateAtomIndexer& indexer = model.getTask().getStateAtomIndexer();
	if (config.getOption<bool>("bfws.extra_features", false)) {
		fs0::bfws::FeatureSelector<State> selector(ProblemInfo::getInstance());
		
		if (selector.has_extra_features()) {
			LPT_INFO("cout", "FEATURE EVALUATION: Extra Features were found!  Using a GenericFeatureSetEvaluator");
			using FeatureEvaluatorT = lapkt::novelty::GenericFeatureSetEvaluator<State>;
			return do_search1<StateModelT, bfws::FSMultivaluedNoveltyEvaluatorI, FeatureEvaluatorT>(model, selector.select(), config, out_dir, start_time, stats);
		}
	}
	
	if (indexer.is_fully_binary()) { // The state is fully binary
		LPT_INFO("cout", "FEATURE EVALUATION: Using the specialized StraightFeatureSetEvaluator<bin>");
		using FeatureEvaluatorT = lapkt::novelty::StraightFeatureSetEvaluator<bool>;
		return do_search1<StateModelT, bfws::FSBinaryNoveltyEvaluatorI, FeatureEvaluatorT>(model, FeatureEvaluatorT(), config, out_dir, start_time, stats);
		
	} else if (indexer.is_fully_multivalued()) { // The state is fully multivalued
		LPT_INFO("cout", "FEATURE EVALUATION: Using the specialized StraightFeatureSetEvaluator<int>");
		using FeatureEvaluatorT = lapkt::novelty::StraightFeatureSetEvaluator<int>;
		return do_search1<StateModelT, bfws::FSMultivaluedNoveltyEvaluatorI, FeatureEvaluatorT>(model, FeatureEvaluatorT(), config, out_dir, start_time, stats);
		
	} else { // We have a hybrid state and cannot thus apply optimizations
		LPT_INFO("cout", "FEATURE EVALUATION: Using a generic StraightHybridFeatureSetEvaluator");
		using FeatureEvaluatorT = lapkt::novelty::StraightHybridFeatureSetEvaluator;
		return do_search1<StateModelT, bfws::FSMultivaluedNoveltyEvaluatorI, FeatureEvaluatorT>(model, FeatureEvaluatorT(), config, out_dir, start_time, stats);
	}
}


//////////////////////////////////////////////////////////////////////


/*
template <typename StateModelT>
typename IteratedWidthDriver<StateModelT>::EnginePT
create(const Config& config, const StateModelT& model) {
	using Engine = FS0IWAlgorithm<StateModelT>;
	using EnginePT = std::unique_ptr<Engine>;
	unsigned max_novelty = config.getOption<int>("width.max");
	return EnginePT(new Engine(model, 1, max_novelty, _stats));
}
*/

template <>
ExitCode 
IteratedWidthDriver<GroundStateModel>::search(Problem& problem, const Config& config, const std::string& out_dir, float start_time) {
	auto model = GroundingSetup::fully_ground_model(problem);
	return do_search(model, config, out_dir, start_time, _stats);
// 	auto engine = create(config, model);
// 	return Utils::do_search(*engine, model, out_dir, start_time, _stats);
}

template <>
ExitCode 
IteratedWidthDriver<LiftedStateModel>::search(Problem& problem, const Config& config, const std::string& out_dir, float start_time) {
	auto model = GroundingSetup::fully_lifted_model(problem);
	return do_search(model, config, out_dir, start_time, _stats);
// 	auto engine = create(config, model);
// 	return Utils::do_search(*engine, model, out_dir, start_time, _stats);
}


} } // namespaces
