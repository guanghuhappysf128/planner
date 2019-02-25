
#pragma once

#include <map>

#include <utils/visitor.hxx>
#include <languages/fstrips/language_fwd.hxx>


//!
//!
//!

namespace fs0 {
	
class State;
class Binding;
// (kind of) forward declarations
using VariableIdx = unsigned;
using ObjectIdx = int;
using PartialAssignment = std::map<VariableIdx, ObjectIdx>;

}

namespace fs0 { namespace language { namespace fstrips {

////////////////////////////////////////////////////////////
//! Returns the index of the state variable to which the given term resolves under the given state.
////////////////////////////////////////////////////////////
template <typename AssignmentT>
VariableIdx interpret_variable(const Term& element, const AssignmentT& assignment, const Binding& binding);

VariableIdx interpret_variable(const Term& element, const State& assignment);
VariableIdx interpret_variable(const Term& element, const PartialAssignment& assignment);

template <typename AssignmentT>
class VariableInterpretationVisitor
    : public Loki::BaseVisitor
    
    , public Loki::Visitor<StateVariable, void, true>
    , public Loki::Visitor<BoundVariable, void, true>
    , public Loki::Visitor<Constant, void, true>
    , public Loki::Visitor<StaticHeadedNestedTerm, void, true>
    , public Loki::Visitor<AxiomaticTermWrapper, void, true>
    , public Loki::Visitor<FluentHeadedNestedTerm, void, true>
    , public Loki::Visitor<UserDefinedStaticTerm, void, true>    
    , public Loki::Visitor<AdditionTerm, void, true>    
	, public Loki::Visitor<SubtractionTerm, void, true>    
	, public Loki::Visitor<MultiplicationTerm, void, true>    
{
public:
	VariableInterpretationVisitor(const AssignmentT& assignment, const Binding& binding) :
		_assignment(assignment), _binding(binding) {}
	
	void Visit(const StateVariable& lhs);
	void Visit(const BoundVariable& lhs) { throw std::runtime_error("Bound variables cannot resolve to an state variable"); }
	void Visit(const Constant& lhs) { throw std::runtime_error("Constant terms cannot resolve to an state variable"); }
	void Visit(const StaticHeadedNestedTerm& lhs) { throw std::runtime_error("Static-headed terms cannot resolve to an state variable"); }
	void Visit(const AxiomaticTermWrapper& lhs) { throw std::runtime_error("Axioms cannot resolve to an state variable"); }
	void Visit(const FluentHeadedNestedTerm& lhs);
	void Visit(const UserDefinedStaticTerm& lhs);
	void Visit(const AdditionTerm& lhs);
	void Visit(const SubtractionTerm& lhs);
	void Visit(const MultiplicationTerm& lhs);
	
	VariableIdx _result;
	
private:
	const AssignmentT& _assignment;
	const Binding& _binding;
};



//! Base interface for all interpreters.
class InterpreterI {
public:
	InterpreterI(const language::fstrips::LogicalElement* root)
		: _root(root)
	{}
	
	//! Return the index of the element in the universe that corresponds to the root, if it is a term,
	//! or a (int-coerced) truth value, if the root is a formula.
	virtual ObjectIdx interpret() = 0;
	
protected:
	const language::fstrips::LogicalElement* _root;
};


//! An interpreter that applies the standard First-Order Logic inductive definition
//! of models and truth - no additional sophistication.
class StandardInterpreter : public InterpreterI {
public:
	StandardInterpreter(const language::fstrips::LogicalElement* root)
		: InterpreterI(root)
	{}
	
	//! Return the index of the element in the universe that corresponds to the root, if it is a term,
	//! or a (int-coerced) truth value, if the root is a formula.
	ObjectIdx interpret() override;
	
protected:
};


ObjectIdx interpret();


} } } // namespaces

