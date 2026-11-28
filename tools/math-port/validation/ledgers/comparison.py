"""Complete source members, review status, mappings, evidence, and file checks."""
from __future__ import annotations

import copy

from ..common import MISSING, ROOT, equal, field, require, truthy
from .targets import target


def source_shape(name, profile, fresh):
    equal(len(profile['inputs']), len(fresh['files']), 'source file count')
    if name in {'map-handler', 'numeric-reductions', 'token-map'}:
        equal(profile['member_counts'], [len(item['members']) for item in fresh['files']],
              'source member counts')
    if name == 'map-handler':
        equal(['register', 'getMap'], [row['name'] for row in fresh['files'][0]['object_methods']],
              'MapHandler object methods')
    if name == 'numeric-reductions':
        equal([['function', 'sum'], ['function', 'max']],
              [[row['kind'], row['name']] for row in fresh['files'][0]['members']], 'numeric functions')
    if name == 'token-map':
        equal(1, sum(row['kind'] == 'initialized_field' for row in fresh['files'][0]['members']),
              'TokenMap initialized fields')


def compare(name, profile, fresh, accepted, *, root=ROOT):
    source_shape(name, profile, fresh)
    equal(len(profile['inputs']), len(accepted['files']), 'accepted file count')
    # These frozen cardinalities are the same as the ten original tools. The
    # source itself still supplies every member, hash, kind, line, and binding.
    equal(profile['member_counts'], [len(item['members']) for item in accepted['files']],
          'accepted member counts')
    members = bindings = 0
    evidence_ids = set()
    for source, reviewed in zip(fresh['files'], accepted['files']):
        file_keys = source.keys() if profile['file_fields'] == 'all' else ['source', 'source_sha256']
        for key in file_keys:
            if key != 'members':
                equal(source[key], field(reviewed, key), f'{source["source"]}.{key}')
        for group in profile.get('groups', ['members']):
            source_rows, accepted_rows = source.get(group, []), reviewed.get(group, [])
            equal(len(source_rows), len(accepted_rows), f'{source["source"]}.{group}.length')
            if group == 'object_methods':
                equal(2, len(accepted_rows), 'MapHandler object method count')
            for source_member, entry in zip(source_rows, accepted_rows):
                label = f'{source["source"]}/{source_member.get("owner", "")}/{source_member["name"]}'
                for key, value in source_member.items():
                    if key not in profile['skip_member_keys']:
                        equal(value, field(entry, key), f'{label}.{key}')
                equal('root_review_passed', field(entry, 'review'), f'{label}.review')
                wanted = field(entry, 'target')
                if profile['target'] == 'mapped':
                    equal(target(name, source['source'], source_member), wanted, f'{label}.target')
                elif profile['target'] == 'string':
                    require(isinstance(wanted, str) and bool(wanted), f'{label}: missing string target')
                else:
                    require(truthy(wanted), f'{label}: missing target')
                if profile.get('target_exists'):
                    require((root / wanted.split(':', 1)[0]).exists(), f'{label}: target file missing: {wanted}')
                # These values preserve historical acceptance identities; they
                # are pinned by the accepted snapshot hash, never dereferenced.
                evidence = field(entry, 'evidence')
                if evidence is not MISSING:
                    require(isinstance(evidence, str), f'{label}: invalid evidence identifier')
                    require(evidence in profile.get('evidence_identifiers', []),
                            f'{label}: evidence identifier is not pinned: {evidence}')
                    evidence_ids.add(evidence)
                if group == 'members':
                    members += 1
                elif group == 'module_bindings':
                    bindings += 1
    if name == 'simple-mml':
        equal(22, members, 'simple MML members')
    equal(sorted(profile.get('evidence_identifiers', [])), sorted(evidence_ids),
          'historical evidence identifiers')
    summary = dict(profile['summary'])
    if name == 'atom-semantics':
        summary.update(members=members, module_bindings=bindings, source_files=len(fresh['files']))
    return summary


def reviewed_copy(name, profile, fresh):
    """Explicit --freeze-reviewed operation; never run by ordinary validation."""
    require('freeze_status' in profile, f'{name} has no reviewed-freeze operation')
    pinned_ids = set(profile.get('evidence_identifiers', []))
    mapped_ids = {member['evidence']
                  for file in profile.get('freeze_files', {}).values()
                  for member in file.get('members', {}).values()
                  if 'evidence' in member}
    require(mapped_ids == pinned_ids, 'Reviewed freeze must retain its pinned historical evidence identifiers')
    for identifier in mapped_ids:
        require(identifier.startswith('docs/math-port/evidence/') and identifier.endswith('.md'),
                f'Invalid historical evidence identifier: {identifier}')
    source_shape(name, profile, fresh)
    result = copy.deepcopy(fresh)
    result['status'] = profile['freeze_status']
    for item in result['files']:
        metadata = profile['freeze_files'][item['source']]
        if 'native_adaptation' in metadata:
            item['native_adaptation'] = metadata['native_adaptation']
        for group in profile.get('groups', ['members']):
            for member in item.get(group, []):
                member['target'] = target(name, item['source'], member)
                member['review'] = 'root_review_passed'
                key = group + '/' + member['owner'] + '/' + member['name']
                member.update(metadata['members'][key])
    return result
