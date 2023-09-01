#! /usr/bin/python3

import sys, os
import toml, json
import argparse as argp

verbose = False

def eprint(*args, **kwargs):
    kwargs['file'] = sys.stderr
    print(*args, **kwargs)

def vprint(*args, **kwargs):
    global verbose
    if verbose:
        print(*args, **kwargs)

def filter_pass_keep_key(func, pair_iterator):
    kept = []
    filtered_keys = []
    for pair in pair_iterator:
        if func(pair):
            kept.append(pair)
        else:
            key, value = pair
            filtered_keys.append(key)
    return kept, filtered_keys 

# Pretty ugly class for parsing out versions from the config file
class Version:

    def __init__(self, val):

        if isinstance(val,int) or isinstance(val,float):
            val = str(val)
        
        if val == None:
            self.major = None
            self.minor = None
            self.patch = None

        elif isinstance(val, str):
            versions = val.split('.')

            if len(versions) > 3 or len(versions) <= 0:
                raise Exception(f'"llvm" string in config file is not correctly formatted! (llvm = "{llvm}")')

            if versions[0] == '*':
                self.major = None
            else:
                self.major = int(versions[0])

            if len(versions) < 2 or versions[1] == '*':
                self.minor = None
            else:
                self.minor = int(versions[1])

            if len(versions) < 3 or versions[2] == '*':
                self.patch = None
            else:
                self.patch == int(versions[2])


        elif isinstance(val, dict):

            if 'major' in val.keys():
                if val['major'] == '*':
                    self.major = None
                else:
                    self.major = int(val['major'])
            else:
                self.major = None

            if 'minor' in val.keys():
                if val['minor'] == '*':
                    self.minor = None
                else:
                    self.minor = int(val['minor'])
            else:
                self.minor = None

            if 'patch' in val.keys():
                if val['patch'] == '*':
                    self.patch = None
                else:
                    self.patch = int(val['patch'])
            else:
                self.patch = None

        else:
            raise TypeError('Version has wrong type (should be "string" or "dict")!')

    def __str__(self):

        if self.major == None:
            major = '*'
        else:
            major = str(self.major)

        if self.minor == None and self.patch != None:
            minor = '.*'
        elif self.minor == None:
            minor = ''
        else:
            minor = '.' + str(self.minor)

        if self.patch == None:
            patch = ''
        else:
            patch = '.' + str(self.patch)

        return f"{major}{minor}{patch}"

    def __eq__(self, other):

        ret = True

        if (self.major != None and other.major != None) and self.major != other.major:
            ret = False
 
        if (self.minor != None and other.minor != None) and self.minor != other.minor:
            ret = False           

        if (self.patch != None and other.patch != None) and self.patch != other.patch:
            ret = False

        return ret

class KconfigFile:

    def __init__(self, path):

        self.path = path
        self.definitions = {}

        with open(path) as file:
            for line in file.readlines():
                uncommented = line.split('#')[0].strip()
                eq_statement = uncommented.split('=')
                if len(eq_statement) != 2:
                    continue
                key = eq_statement[0]
                val = eq_statement[1]
                if key.startswith('NAUT_CONFIG_'):
                    key = key[len('NAUT_CONFIG_'):]
                self.definitions[key.strip()] = val.strip()

    def is_defined(self, key: str):
        return key in self.definitions.keys()

class Pass:

    def __init__(self, directory: str, kconfig: KconfigFile):

        config_table = {}
        
        toml_config_path = os.path.join(directory, 'config.toml')
        json_config_path = os.path.join(directory, 'config.json')

        if os.path.exists(toml_config_path):
            vprint("Found .toml config file")
            config_table = toml.load(toml_config_path)
            path = toml_config_path
        elif os.path.exists(json_config_path):
            vprint("Found .json config file")
            config_table = json.load(json_config_path)
            path = json_config_path
        else:
            eprint(f'Could not find config file in subdirectory: {directory}')
            raise FileNotFoundError

        self.directory = directory
        self.config = config_table

        if 'llvm' in self.config.keys():
            self.llvm_version = Version(self.config['llvm'])
        else:
            self.llvm_version = Version(None)
        
        if 'name' in self.config.keys():
            self.name = self.config['name']
        else:
            self.name = os.path.basename(directory)

        if 'version' in self.config.keys():
            self.version = Version(self.config['version'])
        else:
            self.version = Version(None)

        if 'kconfig' in self.config.keys():
            self.kconfig = self.config['kconfig'].strip()
        else:
            raise Exception(f'Missing "kconfig" field in {path}')

        if not kconfig.is_defined(self.kconfig):
            self.disabled = True
        else:
            self.disabled = False

        self.depends = []
        if 'depends' in self.config.keys():
            if self.config['depends'] == None:
                self.config['depends'] = {}
            if not isinstance(self.config['depends'], dict):
                raise TypeError('Field "depends" in pass config file must be a dict!')
            for name, data in self.config['depends'].items():
                dependency = PassDependency(name, data)
                self.depends.append(dependency)

        vprint(f'Found Pass "{self.name}", Version = {self.version}, LLVM Version = {self.llvm_version}')

class PassDependency:

    def __init__(self, name: str, data: dict):
        self.name = name
        self.topo_sorted = False

        if 'version' in data.keys():
            self.version = Version(data['version'])
        else:
            self.version = Version(None)

def main(args):

    global verbose
    verbose = args.verbose

    if not os.path.exists(args.passes_dir):
        eprint(f"Could not find directory: {args.passes_dir}")
        return -1

    try:
        llvm_version = Version(args.llvm_version)
    except Exception as e:
        eprint(f'Could not parse requested LLVM version: "{args.llvm_version}"!')
        eprint(f'Exception Raised: {e}')
        return -1

    try:
        kconfig_file = KconfigFile(args.kconfig_path)
        if verbose:
            for key, val in kconfig_file.definitions.items():
                vprint(f'Kconfig: {key} = {val}')
    except Exception as e:
        eprint(f'Could not read in Kconfig file: {args.kconfig_path}')
        eprint(f'Exception Raised: {e}')
        return -1

    vprint(f"LLVM Version = {llvm_version}")

    # Collecting passes from subdirectories
    passes = {}

    for file_name in os.listdir(args.passes_dir):
        file_path = os.path.join(args.passes_dir, file_name)
        if os.path.isdir(file_path):
            vprint(f"Looking for pass in subdirectory: {file_path}")
            try:
                new_pass = Pass(file_path, kconfig_file)
                passes[new_pass.name] = new_pass
            except Exception as e:
                vprint(f"Failed to generate pass from subdirectory: {file_path}")
                vprint(f"Exception Raised: {e}")
                continue

    # Apply Filters (For the most part this is just double checking Kconfig has done everything correctly,
    # but Kconfig can't really check for conflicts in version numbers)
    vprint('Filtering Passes')
    filtered_names = {}
    def add_filtered(names, reason):
        for name in names:
            filtered_names[name] = reason

    def disabled_filter(pair):
        name, p = pair
        return not p.disabled

    def llvm_version_filter(pair):
        name, p = pair
        return p.llvm_version == llvm_version

    pass_iter = passes.items()
    pass_iter, disabled_names = filter_pass_keep_key(disabled_filter, pass_iter)
    add_filtered(disabled_names, 'pass was disabled via Kconfig')

    # If passes doesn't contain all of these by the end then something is wrong
    enabled_names = [key for key, value in pass_iter]

    pass_iter, wrong_version_names = filter_pass_keep_key(llvm_version_filter, pass_iter)
    add_filtered(wrong_version_names, 'pass has incompatible LLVM version')

    if verbose:
        for name, reason in filtered_names.items():
            vprint(f'Filtered Pass: "{name}", Reason: {reason}')

    num_missing_enabled = 0
    passes = dict(pass_iter)
    for enabled in enabled_names:
        if enabled not in passes.keys():
            if enabled not in disabled_names.keys():
                eprint(f'Enabled pass: "{enabled}" is not in the filtered list or the final list of passes! (INTERNAL ERROR of "make_passes.py")')
            else:
                reason = disabled_names[enabled]
                eprint(f'Enabled pass: "{enabled}", cannot be applied because: {reason}!')
            num_missing_enabled += 1

    if num_missing_enabled > 0:
        eprint(f'Cannot apply passes because {num_missing_enabled} enabled passes were filtered out!')
        return -1

    vprint('Resolving Pass Dependencies')

    num_failed = 0

    def find_pass_from_dependency(p: Pass, dep: PassDependency):
        nonlocal num_failed
        if not dep.name in passes.keys():
            eprint(f'Missing pass: "{dep.name}", required by pass "{p.name}"!')
            if dep.name in filtered_names.keys():
                reason = filtered_names[dep.name]
                eprint(f'Pass "{dep.name}", exists, however it is incompatible because: {reason}!')
            num_failed += 1
            return None
        
        dep_pass = passes[dep.name]
        
        if dep_pass.version != dep.version:
            eprint(f'Pass "{dep_pass.name}" has version {dep_pass.version}, however pass "{p.name}" requires version {dep.version}!')
            num_failed += 1
            return None

        return dep_pass
            
    for name, p in passes.items():
        p.depends = list(map(lambda dep: find_pass_from_dependency(p, dep), p.depends))

    if num_failed > 0:
        eprint(f'Failed to resolve pass dependencies: num_failed = {num_failed}')
        return -1

    for name, p in passes.items():
        build_pass(name, p)
        apply_pass(name, p)

    return 0

def build_pass(name, p):
    # TODO
    vprint(f"Building pass {name}")
    pass

def apply_pass(name, p):
    # TODO
    vprint(f"Applying pass {name}")
    pass

if __name__ == "__main__":

    parser = argp.ArgumentParser(prog='run')
    parser.add_argument(dest='passes_dir',metavar='[dir]')
    parser.add_argument(dest='in_file', metavar='[in]')
    parser.add_argument('-kc', '--kconfig', dest='kconfig_path', default=os.path.join(os.getcwd(), '.config'))
    parser.add_argument('-o', '--out_file', default=os.getcwd())
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-lv', '--llvm-version', dest='llvm_version', default=None)

    args = parser.parse_args()

    ret = main(args)
    sys.exit(ret)
