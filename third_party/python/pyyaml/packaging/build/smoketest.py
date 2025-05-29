import sys
import yaml


def main():
    # various smoke tests on an installed PyYAML with extension
    if not getattr(yaml, '_yaml', None):
        raise Exception('C extension is not available at `yaml._yaml`')

    print('embedded libyaml version is {0}'.format(yaml._yaml.get_version_string()))

    for loader, dumper in [(yaml.CLoader, yaml.CDumper), (yaml.Loader, yaml.Dumper)]:
        testyaml = 'dude: mar'
        loaded = yaml.load(testyaml, Loader=loader)
        dumped = yaml.dump(loaded, Dumper=dumper)
        if testyaml != dumped.strip():
            raise Exception('roundtrip failed with {0}/{1}'.format(loader, dumper))
    print('smoke test passed for {0}'.format(sys.executable))


if __name__ == '__main__':
    main()