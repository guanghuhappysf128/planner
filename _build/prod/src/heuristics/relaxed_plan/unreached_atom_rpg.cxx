
#include <limits>

#include <languages/fstrips/language.hxx>
#include <languages/fstrips/operations.hxx>
#include <languages/fstrips/scopes.hxx>
#include <heuristics/relaxed_plan/unreached_atom_rpg.hxx>
#include <heuristics/relaxed_plan/relaxed_plan_extractor.hxx>
#include <heuristics/relaxed_plan/rpg_index.hxx>
#include <heuristics/relaxed_plan/relaxed_plan.hxx>
#include <relaxed_state.hxx>
#include <applicability/formula_interpreter.hxx>
#include <constraints/gecode/handlers/base_action_csp.hxx>
#include <constraints/gecode/handlers/ground_effect_csp.hxx>
#include <constraints/gecode/lifted_plan_extractor.hxx>


namespace fs0 { namespace gecode {

UnreachedAtomRPG::UnreachedAtomRPG(const Problem& problem, const fs::Formula* goal_formula, const fs::Formula* state_constraints, std::vector<HandlerPT>&& managers, ExtensionHandler extension_handler) :
	_problem(problem),
	_tuple_index(problem.get_tuple_index()),
	_managers(std::move(managers)),
	_goal_handler(std::unique_ptr<FormulaCSP>(new FormulaCSP(fs::conjunction(*goal_formula, *state_constraints), _tuple_index, false))),
	_extension_handler(extension_handler),
	_atom_achievers(build_achievers_index(_managers, _tuple_index))
{
	LPT_INFO("heuristic", "Unreached-Atom-Based heuristic initialized");
}

//! The actual evaluation of the heuristic value for any given non-relaxed state s.
long UnreachedAtomRPG::evaluate(const State& seed, std::vector<Atom>& relevant) {
	
	if (_problem.getGoalSatManager().satisfied(seed)) return 0; // The seed state is a goal
	
	LPT_EDEBUG("heuristic", std::endl << "Computing RPG from seed state: " << std::endl << seed << std::endl << "****************************************");
	
	RPGIndex graph(seed, _tuple_index, _extension_handler);

	if (Config::instance().useMinHMaxGoalValueSelector()) {
		_goal_handler->init_value_selector(&graph);
	}
	
	auto achieved = graph.achieved_atoms(_tuple_index);
	
	// The main loop - at each iteration we build an additional RPG layer, until no new atoms are achieved (i.e. the rpg is empty), or we reach a goal layer.
	while (true) {
	
		// Begin a new RPG Layer
		
		// cache[i] will contain the CSP corresponding to effect 'i' instantiated to the current layer, or nullptr.
		// We use it to avoid instantiating the same effect CSP more than once per layer
		std::vector<std::unique_ptr<GecodeCSP>> cache(_managers.size());
		
		// failure_cache[i] will be true iff the CSP corresponding to effect 'i' has already been found to be non-applicable in the current layer.
		std::vector<bool> failure_cache(_managers.size(), false);
		
		for (unsigned atom_idx = 0; atom_idx < achieved.size(); ++atom_idx) {
			if (achieved[atom_idx]) continue; // The atom has already been achieved, no need to do anything about it.
// 		for (auto it = unachieved.begin(); it != unachieved.end(); ) {
// 			unsigned atom_idx = *it;
			const Atom& atom = _tuple_index.to_atom(atom_idx);
			
			// Check for a potential support
			bool atom_supported = false;
			for (unsigned manager_idx:_atom_achievers.at(atom_idx)) {
				const HandlerT& manager = *_managers[manager_idx];
				if (failure_cache[manager_idx]) {
					LPT_EDEBUG("heuristic", "Found cached unapplicable effect \"" << *manager.get_effect() << "\" of action \"" << manager.get_action() << "\"");
					continue; // The effect CSP has already been instantiated and found unapplicable on this very same layer
				}
				
				if (cache[manager_idx] == nullptr) {
					GecodeCSP* raw = manager.preinstantiate(graph);
					if (!raw) { // We are instantiating the CSP for the first time in this layer and find that it is not applicable.
						failure_cache[manager_idx] = true;
						LPT_EDEBUG("heuristic", "Effect \"" << *manager.get_effect() << "\" of action \"" << manager.get_action() << "\" inconsistent => not applicable");
						continue;
					}
					cache[manager_idx] = std::unique_ptr<GecodeCSP>(raw);
				} else {
					LPT_EDEBUG("heuristic", "Found cached & applicable effect \"" << *manager.get_effect() << "\" of action \"" << manager.get_action() << "\"");
				}
				
				atom_supported = manager.find_atom_support(atom_idx, atom, seed, *cache[manager_idx], graph);
				if (atom_supported) break; // No need to keep iterating
			}
			
			// If a support was found, no need to check for that particular atom anymore.
			if (atom_supported) {
				LPT_EDEBUG("heuristic", "Found support for atom " << atom);
				achieved[atom_idx] = true;
// 				it = unachieved.erase(it);
			} else {
// 				++it;
			}
		}
		
		// TODO - RETHINK HOW TO FIT THE STATE CONSTRAINTS INTO THIS CSP MODEL
		
		// If there is no novel fact in the rpg, we reached a fixpoint, thus there is no solution.
		if (!graph.hasNovelTuples()) return -1;
		
		
		graph.advance(); // Integrates the novel tuples into the graph as a new layer.
		LPT_EDEBUG("heuristic", "New RPG Layer: " << graph);
		
		long h = computeHeuristic(graph);
		if (h > -1) return h;
		
	}
}

long UnreachedAtomRPG::computeHeuristic(const RPGIndex& graph) {
	return support::compute_rpg_cost(_tuple_index, graph, *_goal_handler);
}


UnreachedAtomRPG::AchieverIndex UnreachedAtomRPG::build_achievers_index(const std::vector<HandlerPT>& managers, const AtomIndex& tuple_index) {
	AchieverIndex index(tuple_index.size()); // Create an index as large as the number of atoms
	
	LPT_INFO("main", "Building index of potential atom achievers");
	
	for (unsigned manager_idx = 0; manager_idx < managers.size(); ++manager_idx) {
		const HandlerPT& manager = managers[manager_idx];
		
		const fs::ActionEffect* effect = manager->get_effect();
		
		// TODO - This uses a very rough overapproximation of the set of potentially affected atoms.
		// A better approach would be to build once, from the initial state, the full RPG, 
		// and extract from there, for each action / effect CSP, the set of atoms
		// that are reached by the CSP in some layer of the RPG.
		
		for (const auto& atom:fs::ScopeUtils::compute_affected_atoms(effect)) {
			AtomIdx idx = tuple_index.to_index(atom);
			index.at(idx).push_back(manager_idx);
		}
	}
	
	return index;
}


} } // namespaces

