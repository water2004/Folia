"""Complete source members, review status, mappings, evidence, and file checks."""
from __future__ import annotations

import copy

from ..common import ROOT, equal, field, require, truthy
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
                if 'evidence' in profile:
                    equal(profile['evidence'], field(entry, 'evidence'), f'{label}.evidence')
                if profile.get('evidence_exists'):
                    evidence = entry['evidence']
                    require(isinstance(evidence, str) and (root / evidence).exists(),
                            f'{label}: evidence file missing: {evidence}')
                if group == 'members':
                    members += 1
                elif group == 'module_bindings':
                    bindings += 1
    if name == 'simple-mml':
        equal(22, members, 'simple MML members')
    summary = dict(profile['summary'])
    if name == 'atom-semantics':
        summary.update(members=members, module_bindings=bindings, source_files=len(fresh['files']))
    return summary


def reviewed_copy(name, profile, fresh, *, root=ROOT):
    """Explicit --freeze-reviewed operation; never run by ordinary validation."""
    require('freeze_status' in profile, f'{name} has no reviewed-freeze operation')
    if 'freeze_evidence_required' in profile:
        require((root / profile['freeze_evidence_required']).exists(), 'Root acceptance evidence required')
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
