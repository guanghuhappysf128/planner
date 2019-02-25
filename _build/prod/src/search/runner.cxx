
#include <iostream>

#include <lapkt/tools/resources_control.hxx>
#include <lapkt/tools/logging.hxx>

#include <problem.hxx>
#include <utils/loader.hxx>
#include <search/runner.hxx>
#include <search/drivers/registry.hxx>
#include <utils/config.hxx>
#include <utils/system.hxx>
#include <problem_info.hxx>
#include <languages/fstrips/language.hxx>
#include <languages/fstrips/operations.hxx>


namespace fs0 { namespace drivers {

Runner::Runner(const EngineOptions& options, ProblemGeneratorType generator) 
	: _options(options), _generator(generator), _start_time(aptk::time_used())
{}

int Runner::run() {
	lapkt::tools::Logger::init(_options.getOutputDir() + "/logs");
	Config::init(_options.getDriver(), _options.getUserOptions(), _options.getDefaultConfigurationFilename());

	std::cout << "Loading problem data" << std::endl;
	//! This will generate the problem and set it as the global singleton instance
	auto data = Loader::loadJSONObject(_options.getDataDir() + "/problem.json");
	Problem* problem = _generator(data, _options.getDataDir());
	const Config& config = Config::instance();
	
	LPT_INFO("main", "Problem instance loaded:" << std::endl << *problem);
	report_stats(*problem, _options.getOutputDir());
	
	LPT_INFO("main", "Planner configuration: " << std::endl << config);
	LPT_INFO("cout", "Deriving control to search engine...");
	
	auto driver = EngineRegistry::instance().get(_options.getDriver());
	ExitCode code = driver->search(*problem, config, _options.getOutputDir(), _start_time);
	report_stats(*problem, _options.getOutputDir()); // Report stats here again so that the number of ground actions, etc. is correctly reported.
	return code;
}

void Runner::report_stats(const Problem& problem, const std::string& out_dir) {
	const ProblemInfo& info = ProblemInfo::getInstance();
	const AtomIndex& tuple_index = problem.get_tuple_index();
	unsigned n_actions = problem.getGroundActions().size();
	std::ofstream json_out( out_dir + "/problem_stats.json" );
	json_out << "{" << std::endl;
	
	unsigned num_goal_atoms = fs::all_atoms(*problem.getGoalConditions()).size();
	unsigned num_sc_atoms = fs::all_atoms(*problem.getStateConstraints()).size();
	
	LPT_INFO("cout", "Number of objects: " << info.getNumObjects());
	LPT_INFO("cout", "Number of state variables: " << info.getNumVariables());
	LPT_INFO("cout", "Number of problem atoms: " << tuple_index.size());
	LPT_INFO("cout", "Number of action schemata: " << problem.getActionData().size());
	LPT_INFO("cout", "Number of (perhaps partially) ground actions: " << n_actions);
	LPT_INFO("cout", "Number of goal atoms: " << num_goal_atoms);
	LPT_INFO("cout", "Number of state constraint atoms: " << num_sc_atoms);
	

	json_out << "\t\"num_objects\": " << info.getNumObjects() << "," << std::endl;
	json_out << "\t\"num_state_variables\": " << info.getNumVariables() << "," << std::endl;
	json_out << "\t\"num_atoms\": " << tuple_index.size() << "," << std::endl;
	json_out << "\t\"num_action_schema\": " << problem.getActionData().size() << "," << std::endl;
	json_out << "\t\"num_grounded_actions\": " << n_actions << "," << std::endl;
	json_out << "\t\"num_goal_atoms\": " << num_goal_atoms << "," << std::endl;
	json_out << "\t\"num_state_constraint_atoms\": " << num_sc_atoms;
	json_out << std::endl << "}" << std::endl;
	json_out.close();
}

} } // namespaces
