"""Exact reviewed native-responsibility mappings from the original inventories."""


def target(profile, source, member):
    owner, name = member['owner'], member['name']
    kind = member.get('kind')
    if profile == 'map-handler':
        cpp, ixx = 'src/math/input/tex/map_handler.cpp:', 'src/math/input/tex/map_handler.ixx:'
        if owner == 'MapHandler':
            return cpp + 'MapHandler::' + ('registerMap' if name == 'register' else name)
        if owner == '<module>':
            return cpp + 'mapRegistry' if name == 'maps' else ixx + 'MapHandler'
        if name == 'FALLBACK':
            return cpp + 'SubHandler::FALLBACK'
        if name == '_configuration':
            return ixx + 'SubHandler::configuration_'
        if name == '_fallback':
            return ixx + 'SubHandler::fallback_'
        if owner == 'SubHandlers' and name == 'map':
            return ixx + 'SubHandlers::map_'
        return (ixx if name == 'keys' else cpp) + owner + '::' + name
    if profile == 'mathchoice':
        suffix = ('.cpp:MathChoice::inheritAttributes' if name == 'setInheritedAttributes'
                  else '_bindings.cpp:MathChoice::' + name)
        return 'src/math/core/mml_tree/mml_nodes/mathchoice' + suffix
    if profile == 'mml-action':
        if source.endswith('/MML.ts'):
            return 'src/math/core/mml_tree/mml.cpp:folia::math::mml::MML'
        if source.endswith('/MmlFactory.ts'):
            return 'src/math/core/mml_tree/mml_factory.' + (
                'ixx:MmlFactory::defaultNodes' if name == 'defaultNodes' else 'cpp:MmlFactory::MML')
        unit = ('selection' if name in {'selected', 'isEmbellished', 'isSpacelike', 'core', 'coreMO'}
                else 'bindings' if name in {'defaults', 'kind', 'arity'} else 'behavior')
        return f'src/math/core/mml_tree/mml_nodes/maction_{unit}.cpp:MmlMaction::{name}'
    if profile == 'numeric-reductions':
        return 'src/math/util/numeric.cpp:folia::math::numeric::' + name
    if profile == 'tex-token':
        method = owner if name == 'constructor' else 'character' if name == 'char' else name
        return (f'src/math/input/tex/{owner.lower()}.cpp:{owner}::{method}' if source.endswith('Token.ts')
                else 'src/math/input/tex/tex_constants.ixx:TexConstant')
    if profile == 'token-map':
        if owner == '<module>':
            return 'src/math/input/tex/token_map.cpp:parseResult'
        if kind == 'initialized_field':
            return 'src/math/input/tex/token_map.cpp:AbstractParseMap::initializeSource [map_ field initialization]'
        method = 'initializeSource' if kind == 'constructor' else name
        suffix = ' [getter]' if kind == 'get' else ' [setter]' if kind == 'set' else ''
        return f'src/math/input/tex/token_map.cpp:{owner}::{method}{suffix}'
    raise ValueError(f'No reviewed target mapping for {profile}')
