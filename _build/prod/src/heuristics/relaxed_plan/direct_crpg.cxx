
#include <limits>
#include <numeric>

#include <heuristics/relaxed_plan/direct_crpg.hxx>
#include <constraints/direct/action_manager.hxx>
#include <heuristics/relaxed_plan/relaxed_plan_extractor.hxx>
#include <relaxed_state.hxx>
#include <applicability/formula_interpreter.hxx>


namespace fs0 {

DirectCRPG::DirectCRPG(const Problem& problem, std::vector<std::unique_ptr<DirectActionManager>>&& managers, std::shared_ptr<DirectRPGBuilder> builder) :
	_problem(problem), _managers(std::move(managers)), all_whitelist(_managers.size()), _builder(builder), _last_extractor(nullptr)
{
	LPT_DEBUG("heuristic", "Relaxed Plan heuristic initialized with builder: " << std::endl << *_builder);
	std::iota(all_whitelist.begin(), all_whitelist.end(), 0); // Fill in whe vector with values 0, 1, 2, 3 ...
}

long DirectCRPG::evaluate(const State& seed, std::vector<Atom>& relevant) {
	return evaluate(seed, all_whitelist); // If no whitelist is provided, all actions are considered.
}

//! The actual evaluation of the heuristic value for any given non-relaxed state s.
long DirectCRPG::evaluate(const State& seed, const std::vector<ActionIdx>& whitelist) {
	
	if (_problem.getGoalSatManager().satisfied(seed)) return 0; // The seed state is a goal
	
	RelaxedState relaxed(seed);
	RPGData bookkeeping(seed);
	
	LPT_EDEBUG("heuristic", std::endl << "Computing RPG from seed state: " << std::endl << seed << std::endl << "****************************************");
	
	// The main loop - at each iteration we build an additional RPG layer, until no new atoms are achieved (i.e. the rpg is empty),
	// or we get to a goal graph layer.
	while(true) {
		// Apply all the actions to the RPG layer
		for (unsigned idx:whitelist) {
			const auto& manager = _managers[idx];
			LPT_EDEBUG("heuristic", "Processing ground action #" << idx << ": " << print::action_header(manager->getAction()));
			manager->process(idx, relaxed, bookkeeping);
		}
		
		LPT_EDEBUG("heuristic", "The last layer of the RPG contains " << bookkeeping.getNumNovelAtoms() << " novel atoms." << std::endl << bookkeeping);
		
		// If there is no novel fact in the rpg, we reached a fixpoint, thus there is no solution.
		if (bookkeeping.getNumNovelAtoms() == 0) {
			return -1;
		}
		
		// unsigned prev_number_of_atoms = relaxed.getNumberOfAtoms();
		relaxed.accumulate(bookkeeping.getNovelAtoms());
		LPT_EDEBUG("heuristic", "RPG Layer #" << bookkeeping.getCurrentLayerIdx() << ": " << relaxed);
		
/*
 * RETHINK HOW TO FIT THE STATE CONSTRAINTS INTO THE CSP MODEL
 		
		// Prune using state constraints - TODO - Would be nicer if the whole state constraint pruning was refactored into a single line
		FilteringOutput o = _builder.pruneUsingStateConstraints(relaxed);
		LPT_EDEBUG("heuristic", "State Constraint pruning output: " <<  static_cast<std::underlying_type<FilteringOutput>::type>(o));
		if (o == FilteringOutput::Failure) return std::numeric_limits<unsigned>::max();
		if (o == FilteringOutput::Pruned && relaxed.getNumberOfAtoms() <= prev_number_of_atoms) return std::numeric_limits<float>::infinity();
*/
		
		
		
		long h = computeHeuristic(seed, relaxed, bookkeeping);
		if (h > -1) return h;
		
		bookkeeping.advanceLayer();
	}
}

long DirectCRPG::computeHeuristic(const State& seed, const RelaxedState& state, const RPGData& bookkeeping) {
	Atom::vctr causes;
	if (_builder->isGoal(seed, state, causes)) {
		_last_extractor = std::unique_ptr<BaseRelaxedPlanExtractor<RPGData>>(RelaxedPlanExtractorFactory<RPGData>::create(seed, bookkeeping));
		long cost = _last_extractor->computeRelaxedPlanCost(causes);
		return cost;
	} else return -1;
}



DirectCHMax::DirectCHMax(const Problem& problem, std::vector<std::unique_ptr<DirectActionManager>>&& managers, std::shared_ptr<DirectRPGBuilder> builder)
	: DirectCRPG(problem, std::move(managers), std::move(builder))
{}

long DirectCHMax::computeHeuristic(const State& seed, const RelaxedState& state, const RPGData& bookkeeping) {
		if (this->_builder->isGoal(state)) return bookkeeping.getCurrentLayerIdx();
		return -1;
}
} // namespaces

