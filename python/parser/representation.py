"""
    A particular problem representation combining json-encoded data plus automatically-generated C++ code.
"""
import json
import operator

from python import utils
from . import fstrips
from . import util
from .static import DataElement
from .templates import tplManager


class ProblemRepresentation(object):

    def __init__(self, index, translation_dir, debug):
        self.index = index
        self.translation_dir = translation_dir
        self.debug = debug

    def generate(self):

        data = {'variables': self.dump_variable_data(),
                'objects': self.dump_object_data(),
                'types': self.dump_type_data(),
                'action_schemata': [action.dump() for action in self.index.action_schemas],
                'state_constraints': self.index.state_constraints.dump(),
                'goal': self.index.goal.dump(),
                'axioms': [axiom.dump() for axiom in self.index.axioms],
                'init': self.dump_init_data(),
                'symbols': self.dump_symbol_data(),

                'problem': {'domain': self.index.domain_name, 'instance': self.index.instance_name}
                }

        self.dump_data('problem', json.dumps(data), ext='json')

        # Optionally, we'll want to print out the precomputed action groundings
        self.print_groundings_if_available(data['action_schemata'], self.index.groundings, self.index.objects)
        self.print_debug_data(data)
        self.serialize_static_extensions()

        if self.requires_compilation():
            self.generate_components_code()

    def generate_components_code(self):
        # components.hxx:
        self.save_translation('components.hxx', tplManager.get('components.hxx').substitute(
            method_factories=self.get_method_factories(),
        ))

        # components.cxx:
        self.save_translation('components.cxx', tplManager.get('components.cxx').substitute())

    def serialize_static_extensions(self):
        for elem in self.index.initial_static_data.values():
            assert isinstance(elem, DataElement)
            serialized = elem.serialize_data(self.index.objects.data)
            self.dump_data(elem.name, serialized)

    def get_method_factories(self):
        return tplManager.get('factories').substitute(
            functions=',\n\t\t\t'.join(self.get_function_instantiations()),
        )

    def dump_init_data(self):
        """ Saves the initial values of all state variables explicitly mentioned in the initialization """

        def indexer(atom):  # A small helper
            return [self.index.state_variables.get_index(atom.var), self.get_value_idx(atom.value)]

        indexed_atoms = [indexer(atom) for atom in self.index.initial_fluent_atoms]
        sorted_atoms = sorted(indexed_atoms)
        return dict(variables=len(self.index.state_variables), atoms=sorted_atoms)

    def dump_variable_data(self):
        res = []
        for i, var in enumerate(self.index.state_variables):
            data = self.dump_state_variable(var)
            res.append({'id': i, 'name': str(var), 'type': self.index.symbol_types[var.symbol], 'data': data})
        return res

    def dump_state_variable(self, var):
        head = self.index.symbol_index[var.symbol]
        constants = [arg if utils.is_int(arg) else self.index.objects.get_index(arg) for arg in var.args]
        return [head, constants]

    def dump_object_data(self):
        return [{'id': i, 'name': obj} for i, obj in enumerate(self.index.objects.dump())]

    def dump_symbol_data(self):
        res = []
        for name, symbol in self.index.symbols.items():
            i = self.index.symbol_index[name]

            type_ = "predicate" if isinstance(symbol, fstrips.Predicate) else "function"

            # Collect the list of variables that arise from this particular symbol
            f_variables = [(i, str(v)) for i, v in enumerate(self.index.state_variables) if v.symbol == name]

            static = name in self.index.static_symbols
            unbounded = util.has_unbounded_arity(name)

            # Store the symbol info as a tuple:
            # <ID, name, type, <function_domain>, function_codomain, state_variables, static?, unbounded_arity?>
            res.append([i, name, type_, symbol.arguments, symbol.codomain, f_variables, static, unbounded])
        return res

    def dump_type_data(self):
        """ Dumps a map of types to corresponding objects"""
        data = []
        _sorted = sorted(self.index.types.items(), key=operator.itemgetter(1))  # all types, sorted by type ID
        for t, i in _sorted:
            objects = self.index.type_map[t]
            if objects and isinstance(objects[0], int):  # We have a bounded int variable
                data.append([i, t, 'int', [objects[0], objects[-1]]])
            else:
                object_idxs = [str(self.index.objects.get_index(o)) for o in objects]
                data.append([i, t, object_idxs])

        return data

    def save_translation(self, name, translation):
        with open(self.translation_dir + '/' + name, "w") as f:
            f.write(translation)

    def _compute_filenames(self, name, ext, subdir=None):
        basedir = self.translation_dir + '/data'

        if subdir:
            basedir += '/' + subdir

        return basedir, basedir + '/' + name + '.' + ext

    def dump_data(self, name, data, ext='data', subdir=None):
        if not isinstance(data, list):
            data = [data]

        basedir, filename = self._compute_filenames(name, ext, subdir)
        utils.mkdirp(basedir)
        with open(filename, "w") as f:
            for l in data:
                f.write(str(l) + '\n')

    def rm_data(self, name, ext='data', subdir=None):
        basedir, filename = self._compute_filenames(name, ext, subdir)
        utils.silentremove(filename)

    def get_value_idx(self, value):
        """ Returns the appropriate integer index for the given value."""
        if isinstance(value, int):  # Variables of integer type are represented by the integer itself.
            return value
        else:
            # bool variables also need to be treated specially
            value = util.bool_string(value) if isinstance(value, bool) else value
            return self.index.objects.get_index(value)

    def requires_compilation(self):
        # The problem requires compilation iff there are external symbols involved.
        return len([s for s in self.index.all_symbols if util.is_external(s)])

    def get_function_instantiations(self):
        return [tplManager.get('function_instantiation').substitute(name=symbol, accessor=symbol.replace('@', ''))
                for symbol in self.index.static_symbols if util.is_external(symbol)]

    def print_debug_data(self, data):
        if not self.debug:
            return

        self.dump_data('debug.problem', json.dumps(data, indent=2), ext='json', subdir='debug')
        # self.dump_data('debug.problem', pprint.pformat(data, width=30, indent=2), ext='txt', subdir='debug')

        # For ease of inspection we dump each component into a separate JSON file
        for k, elem in data.items():
            self.dump_data("{}".format(k), json.dumps(elem, indent=2), ext='json', subdir='debug')

        # And further separate each action into a different file:
        for action in data['action_schemata']:
            self.dump_data("action.{}".format(action['name']), json.dumps(action, indent=2), ext='json', subdir='debug')

        # Separate each axiom into a different file:
        for axiom in data['axioms']:
            self.dump_data("axiom.{}".format(axiom['name']), json.dumps(axiom, indent=2), ext='json', subdir='debug')

        # Print all (ordered) action schema names in a separate file
        names = [action['name'] for action in data['action_schemata']]
        self.dump_data("schemas", names, ext='txt', subdir='debug')

        # Print all (ordered) axioms in a separate file
        axiom_data = ["{}\n\n".format(axiom) for axiom in self.index.axioms]
        self.dump_data("axioms", axiom_data, ext='txt', subdir='debug')

        # Types
        lines = list()
        for t in data['types']:
            objects = t[2]
            if objects == 'int':
                objects = "int[{}..{}]".format(t[3][0], t[3][1])
            lines.append("{}: {}. Objects: {}".format(t[0], t[1], objects))
        self.dump_data("types", lines, ext='txt', subdir='debug')

        # Variables
        lines = list()
        for elem in data['variables']:
            lines.append('{}: "{}" ({})'.format(elem['id'], elem['name'], elem['type']))
        self.dump_data("variables", lines, ext='txt', subdir='debug')

        # Symbols
        lines = list()
        for elem in data['symbols']:
            signature = ', '.join(elem[3])
            lines.append('{}: "{}" ({}). Signature: ({})'.format(elem[0], elem[1], elem[2], signature))
        self.dump_data("symbols", lines, ext='txt', subdir='debug')

        # Objects
        lines = list()
        for elem in data['objects']:
            lines.append('{}: "{}"'.format(elem['id'], elem['name']))
        self.dump_data("objects", lines, ext='txt', subdir='debug')

        # Init
        lines = list()
        for elem in data['init']['atoms']:
            lines.append('Variable {} has value "{}"'.format(elem[0], elem[1]))
        self.dump_data("init", lines, ext='txt', subdir='debug')

    def print_groundings_if_available(self, schemas, all_groundings, object_idx):
        groundings_filename = "groundings"

        if all_groundings is None:  # No groundings available
            self.rm_data(groundings_filename)
            return

        data = []

        # Order matters! The groundings of each action schema are provided in consecutive blocks, one grounding per line
        for i, action in enumerate(schemas, 0):
            action_name = action['name']
            data.append("# {} # {}".format(i, action_name))  # A comment line

            action_groundings = []  # A list with the (integer index of) each grounding
            for grounding in all_groundings[action_name]:
                action_groundings.append(tuple(object_idx.get_index(obj_name) for obj_name in grounding))

            for grounding in sorted(action_groundings):  # IMPORTANT to output the groundings in lexicographical order
                data.append(','.join(map(str, grounding)))

        self.dump_data(groundings_filename, data)
