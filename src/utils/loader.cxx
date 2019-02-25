
#include <memory>

#include <problem.hxx>
#include <utils/loader.hxx>
#include <actions/actions.hxx>
#include <actions/grounding.hxx>
#include <utils/component_factory.hxx>
#include <languages/fstrips/loader.hxx>
#include <languages/fstrips/axioms.hxx>
#include <lapkt/tools/logging.hxx>
#include <constraints/gecode/helper.hxx>
#include <constraints/registry.hxx>
#include <utils/printers/registry.hxx>
#include <utils/config.hxx>
#include <utils/static.hxx>
#include <state.hxx>
#include <problem_info.hxx>
#include <languages/fstrips/formulae.hxx>
#include <languages/fstrips/terms.hxx>
#include <languages/fstrips/operations.hxx>
#include <validator.hxx>


namespace fs = fs0::language::fstrips;

namespace fs0 {
	
std::unordered_map<std::string, const fs::Axiom*>
_index_axioms(const std::vector<const fs::Axiom*>& axioms) {
	std::unordered_map<std::string, const fs::Axiom*> index;
	for (const fs::Axiom* axiom:axioms) {
		index.insert(std::make_pair(axiom->getName(), axiom));
	}
	return index;
}

// UGLY HACK. This should not be here.
bool
_check_negated_preconditions(std::vector<const ActionData*>& schemas) {
	for (const ActionData* schema:schemas) {
		for (const fs::AtomicFormula* atom:fs::all_atoms(*schema->getPrecondition())) {
			const fs::EQAtomicFormula* eq = dynamic_cast<const fs::EQAtomicFormula*>(atom);
			if (!eq) continue;
			const fs::IntConstant* cnst = dynamic_cast<const fs::IntConstant*>(eq->rhs());
			if (!cnst) continue;
			auto val = cnst->getValue();
			if (val==0) return true;
		}
	}
	
	return false;
}

Problem* Loader::loadProblem(const rapidjson::Document& data) {
	const Config& config = Config::instance();
	const ProblemInfo& info = ProblemInfo::getInstance();
	
	LPT_INFO("main", "Creating State Indexer...");
	auto indexer = StateAtomIndexer::create(info);
	
	LPT_INFO("main", "Loading initial state...");
	auto init = loadState(*indexer, data["init"]);
	
	LPT_INFO("main", "Loading action data...");
	auto action_data = loadAllActionData(data["action_schemata"], info, true);
	
	LPT_INFO("main", "Loading axiom data...");
	// Axiom schemas are simply action schemas but without effects
	auto axioms = loadAxioms(data["axioms"], info);
	auto axiom_idx = _index_axioms(axioms);
	
	LPT_INFO("main", "Loading goal formula...");
	auto goal = loadGroundedFormula(data["goal"], info);
	
	LPT_INFO("main", "Loading state constraints...");
	auto sc = loadGroundedFormula(data["state_constraints"], info);
	
	//! Set the global singleton Problem instance
	bool has_negated_preconditions = _check_negated_preconditions(action_data);
	LPT_INFO("cout", "Quick Negated-Precondition Test: Does the problem have negated preconditions? " << has_negated_preconditions);
	Problem* problem = new Problem(init, indexer, action_data, axiom_idx, goal, sc, AtomIndex(info, has_negated_preconditions));
	Problem::setInstance(std::unique_ptr<Problem>(problem));
	
	problem->consolidateAxioms();
	
	LPT_INFO("components", "Bootstrapping problem with following external component repository\n" << print::logical_registry(LogicalComponentRegistry::instance()));

	if (config.validate()) {
		LPT_INFO("main", "Validating problem...");
		Validator::validate_problem(*problem, info);
	}
	
	return problem;
}

void 
Loader::loadFunctions(const BaseComponentFactory& factory, ProblemInfo& info) {
	
	// First load the extensions of the static symbols
	for (auto name:info.getSymbolNames()) {
		unsigned id = info.getSymbolId(name);
		if (info.getSymbolData(id).isStatic()) {
			info.set_extension(id, StaticExtension::load_static_extension(name, info));
		}
	}
	
	// Load the function objects for externally-defined symbols
	for (auto elem:factory.instantiateFunctions(info)) {
		info.setFunction(info.getSymbolId(elem.first), elem.second);
	}
}

ProblemInfo&
Loader::loadProblemInfo(const rapidjson::Document& data, const std::string& data_dir, const BaseComponentFactory& factory) {
	// Load and set the ProblemInfo data structure
	auto info = std::unique_ptr<ProblemInfo>(new ProblemInfo(data, data_dir));
	loadFunctions(factory, *info);
	return ProblemInfo::setInstance(std::move(info));
}

State*
Loader::loadState(const StateAtomIndexer& indexer, const rapidjson::Value& data) {
	// The state is an array of two-sized arrays [x,v], representing atoms x=v
	unsigned numAtoms = data["variables"].GetInt();
	Atom::vctr facts;
	for (unsigned i = 0; i < data["atoms"].Size(); ++i) {
		const rapidjson::Value& node = data["atoms"][i];
		facts.push_back(Atom(node[0].GetInt(), node[1].GetInt()));
	}
	return State::create(indexer, numAtoms, facts);
}


std::vector<const ActionData*>
Loader::loadAllActionData(const rapidjson::Value& data, const ProblemInfo& info, bool load_effects) {
	std::vector<const ActionData*> schemata;
	for (unsigned i = 0; i < data.Size(); ++i) {
		if (const ActionData* adata = loadActionData(data[i], i, info, load_effects)) {
			schemata.push_back(adata);
		}
	}
	return schemata;
}

std::vector<const fs::Axiom*>
Loader::loadAxioms(const rapidjson::Value& data, const ProblemInfo& info) {
	std::vector<const fs::Axiom*> axioms;
	for (const ActionData* action:loadAllActionData(data, info, false)) {
		axioms.push_back(new fs::Axiom(action->getName(), action->getSignature(), action->getParameterNames(), action->getBindingUnit(), action->getPrecondition()->clone()));
		delete action;
	}
	return axioms;
}

const ActionData*
Loader::loadActionData(const rapidjson::Value& node, unsigned id, const ProblemInfo& info, bool load_effects) {
	const std::string& name = node["name"].GetString();
	const Signature signature = parseNumberList<unsigned>(node["signature"]);
	const std::vector<std::string> parameters = parseStringList(node["parameters"]);
	const fs::BindingUnit unit(parameters, fs::Loader::parseVariables(node["unit"], info));
	
	const fs::Formula* precondition = fs::Loader::parseFormula(node["conditions"], info);
	std::vector<const fs::ActionEffect*> effects;
	
	if (load_effects) {
		effects = fs::Loader::parseEffectList(node["effects"], info);
	}
	
	ActionData adata(id, name, signature, parameters, unit, precondition, effects);
	if (adata.has_empty_parameter()) {
		LPT_INFO("cout", "Schema \"" << adata.getName() << "\" discarded because of empty parameter type.");
		return nullptr;
	}
	
	// We perform a first binding on the action schema so that state variables, etc. get consolidated, but the parameters remain the same
	// This is possibly not optimal, since for some configurations we might be duplicating efforts, but ATM we are happy with it
	return ActionGrounder::process_action_data(adata, info, load_effects);
}


const fs::Formula*
Loader::loadGroundedFormula(const rapidjson::Value& data, const ProblemInfo& info) {
	const fs::Formula* unprocessed = fs::Loader::parseFormula(data["conditions"], info);
	// The conditions are by definition already grounded, and hence we need no binding, but we process the formula anyway
	// to detect tautologies, contradictions, etc., and to consolidate state variables
	auto processed = fs::bind(*unprocessed, Binding::EMPTY_BINDING, info);
	delete unprocessed;
	return processed;
}

rapidjson::Document
Loader::loadJSONObject(const std::string& filename) {
	// Load and parse the JSON data file.
	std::ifstream in(filename);
	if (in.fail()) throw std::runtime_error("Could not open filename '" + filename + "'");
	std::string str((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	rapidjson::Document data;
	data.Parse(str.c_str());
	return data;
}


std::vector<std::string>
Loader::parseStringList(const rapidjson::Value& data) {
	std::vector<std::string> output;
	for (unsigned i = 0; i < data.Size(); ++i) {
		output.push_back(data[i].GetString());
	}
	return output;
}


template<typename T>
std::vector<std::vector<T>> Loader::parseDoubleNumberList(const rapidjson::Value& data) {
	std::vector<std::vector<T>> output;
	assert(data.IsArray());
	if (data.Size() == 0) {
		output.push_back(std::vector<T>());
	} else {
		for (unsigned i = 0; i < data.Size(); ++i) {
			output.push_back(parseNumberList<T>(data[i]));
		}
	}
	return output;
}

} // namespaces
