
#include <lapkt/tools/logging.hxx>

#include <languages/fstrips/light_loader.hxx>
#include <languages/fstrips/light_operations.hxx>
#include <languages/fstrips/light.hxx>
#include <utils/loader.hxx>

#include <problem_info.hxx>
#include <languages/fstrips/builtin.hxx>
#include <constraints/registry.hxx>


namespace fs0 { namespace lang { namespace fstrips {
	

	
std::vector<const LogicalVariable*> 
_parseVariables(const rapidjson::Value& tree, const ProblemInfo& info) {
	std::vector<const LogicalVariable*> list;
	for (unsigned i = 0; i < tree.Size(); ++i) {
		const rapidjson::Value& node = tree[i];
		unsigned id = node[0].GetUint();
 		std::string name = node[1].GetString();
		std::string type_name = node[2].GetString();
		TypeIdx type = info.getTypeId(type_name);
		list.push_back(new LogicalVariable(id, name, type));
	}
	return list;
}

//! Parse a list of terms
std::vector<const Term*> _parseTermList(const rapidjson::Value& tree, const ProblemInfo& info) {
	std::vector<const Term*> list;
	for (unsigned i = 0; i < tree.Size(); ++i) {
		list.push_back(Loader::parseTerm(tree[i], info));
	}
	return list;
}


const Formula* Loader::parseFormula(const rapidjson::Value& tree, const ProblemInfo& info) {
	std::string formula_type = tree["type"].GetString();
	
	if (formula_type == "and" || formula_type == "or" || formula_type == "not") {
		std::vector<const Formula*> subformulae;
		const rapidjson::Value& children = tree["children"];
		for (unsigned i = 0; i < children.Size(); ++i) {
			subformulae.push_back(parseFormula(children[i], info));
		}
		
		return new OpenFormula(to_connective(formula_type), subformulae);
	
		
	} else if (formula_type == "exists" || formula_type == "forall") {
		std::vector<const LogicalVariable*> variables = _parseVariables(tree["variables"], info);
		auto subformula = parseFormula(tree["subformula"], info);
		
		return new QuantifiedFormula(to_quantifier(formula_type), variables, subformula);
	
	} else if (formula_type == "atom") {
		std::string symbol = tree["symbol"].GetString();
		unsigned symbol_id = info.getSymbolId(symbol);
		bool negated = tree["negated"].GetBool(); // TODO NEGATED SHOULDN'T BE HERE, BUT RATHER A NEGATION.
		std::vector<const Term*> subterms = _parseTermList(tree["children"], info);
		
		return new AtomicFormula(symbol_id, subterms);

	} else if (formula_type == "tautology") {
		return new Tautology;
	} else if (formula_type == "contradiction") {
		return new Contradiction;
	}
	
	throw std::runtime_error("Unknown formula type \"" + formula_type + "\"");
}


const Term* Loader::parseTerm(const rapidjson::Value& tree, const ProblemInfo& info) {
	std::string term_type = tree["type"].GetString();
	
	if (term_type == "constant" || term_type == "int_constant") {
		return new Constant(tree["value"].GetInt(), info.getTypeId(tree["typename"].GetString()));

	} else if (term_type == "variable") {
		return new LogicalVariable(tree["position"].GetInt(), tree["name"].GetString(), info.getTypeId(tree["typename"].GetString()));
		
	} else if (term_type == "functional") {
		std::string symbol = tree["symbol"].GetString();
		unsigned symbol_id = info.getSymbolId(symbol);
		std::vector<const Term*> children = _parseTermList(tree["children"], info);
		return new FunctionalTerm(symbol_id, children);
	}
	
	throw std::runtime_error("Unknown term type " + term_type);
}





const ActionEffect* Loader::parseEffect(const rapidjson::Value& tree, const ProblemInfo& info) {
	const std::string effect_type = tree["type"].GetString();
	const Formula* condition = parseFormula(tree["condition"], info);
	
	if (effect_type == "functional") {
		const FunctionalTerm* lhs = dynamic_cast<const FunctionalTerm*>(parseTerm(tree["lhs"], info));
		if (!lhs) {
			throw std::runtime_error("Invalid LHS of a functional effect");
		}
		
		return new FunctionalEffect(lhs, parseTerm(tree["rhs"], info), condition);

	} else if (effect_type == "add" || effect_type == "del") {
		AtomicEffect::Type type = AtomicEffect::to_type(effect_type);
		const AtomicFormula* atom = dynamic_cast<const AtomicFormula*>(parseFormula(tree["lhs"], info));
		if (!atom) {
			throw std::runtime_error("Invalid LHS of an atomic effect");
		}
		
		return new AtomicEffect(atom, type, condition);
		
	} 
	
	throw std::runtime_error("Unknown effect type " + effect_type);	
}

std::vector<const ActionEffect*> Loader::parseEffectList(const rapidjson::Value& tree, const ProblemInfo& info) {
	std::vector<const ActionEffect*> list;
	for (unsigned i = 0; i < tree.Size(); ++i) {
		list.push_back(parseEffect(tree[i], info));
	}
	return list;
}


const ActionSchema*
Loader::parseActionSchema(const rapidjson::Value& node, unsigned id, const ProblemInfo& info, bool load_effects) {
	const std::string& name = node["name"].GetString();
	const Signature signature = fs0::Loader::parseNumberList<unsigned>(node["signature"]);
	const std::vector<std::string> parameters = fs0::Loader::parseStringList(node["parameters"]);
	
	const Formula* precondition = parseFormula(node["conditions"], info);
	std::vector<const ActionEffect*> effects;
	
	if (load_effects) {
		effects = parseEffectList(node["effects"], info);
	}
	
	ActionSchema* schema = new ActionSchema(id, name, signature, parameters, precondition, effects);
	if (has_empty_parameter(*schema)) {
		LPT_INFO("cout", "Schema \"" << schema->getName() << "\" discarded because of empty parameter type.");
		delete schema;
		return nullptr;
	}
	
	return schema;
	// We perform a first binding on the action schema so that state variables, etc. get consolidated, but the parameters remain the same
	// This is possibly not optimal, since for some configurations we might be duplicating efforts, but ATM we are happy with it
// 	return ActionGrounder::process_action_data(adata, info, load_effects);
}


} } } // namespaces
