import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT
from validation.common_passive import EVENTS, observations
from validation.texhtml import META_KEYS, DOM_KEYS, TexHtmlValidation


def texhtml_fixture():
    meta = dict(zip(META_KEYS, [1, 'MathJax 4.1.3', 'pinned', 'scope', [{'file': 'source', 'sha256': 'hash'}]]))
    tex = meta | {'cases': [{'name': 'element', 'result': {'tag': 'span', 'attrs': {'id': 'x', 'style': 'color:red'}, 'children': ['µ']}}]}
    dom = meta | {key: {'value': index} for index, key in enumerate(DOM_KEYS)}
    xml = meta | {'cases': [{'name': 'escape', 'xml': '&lt;&amp;'}, {'name': 'child', 'xml': '<node>🙂</node>'}]}
    native_tex = meta | {'texhtml': {'cases': copy.deepcopy(tex['cases'])},
                         'domStrings': {'cases': [{key: dom[key] for key in DOM_KEYS[:-1]}, {'nestedMarkers': dom['nestedMarkers']}]}}
    return tex, dom, xml, copy.deepcopy(native_tex), {'cases': copy.deepcopy(xml['cases'])}


class TexHtmlAndPassiveTests(unittest.TestCase):
    def test_complete_texhtml_xml_envelopes_and_dom_groups(self):
        values = texhtml_fixture()
        check = TexHtmlValidation(ROOT / 'docs/math-port/evidence/texhtml-xml-source-lock.json')
        check.observations(*values)
        self.assertEqual(check.failures, [])
        for mutation in ['meta', 'envelope', 'tex-extra-field', 'tex-missing', 'xml-extra', 'dom-count', 'dom-key-order', 'dom-value', 'group-extra']:
            with self.subTest(mutation=mutation):
                tex, dom, xml, native_tex, native_xml = copy.deepcopy(values)
                if mutation == 'meta':
                    native_tex['scopeSha256'] = 'different'
                elif mutation == 'envelope':
                    native_tex = dict(reversed(list(native_tex.items())))
                elif mutation == 'tex-extra-field':
                    native_tex['texhtml']['cases'][0]['extra'] = None
                elif mutation == 'tex-missing':
                    native_tex['texhtml']['cases'].clear()
                elif mutation == 'xml-extra':
                    native_xml['cases'].append({'name': 'unexpected'})
                elif mutation == 'dom-count':
                    native_tex['domStrings']['cases'].pop()
                elif mutation == 'dom-key-order':
                    native_tex['domStrings']['cases'][0] = dict(reversed(list(native_tex['domStrings']['cases'][0].items())))
                elif mutation == 'dom-value':
                    native_tex['domStrings']['cases'][1]['nestedMarkers']['value'] += 1e-12
                else:
                    native_tex['domStrings']['extra'] = None
                check = TexHtmlValidation(ROOT / 'docs/math-port/evidence/texhtml-xml-source-lock.json')
                check.observations(tex, dom, xml, native_tex, native_xml)
                self.assertTrue(check.failures)

    def test_all75_common_rows_and_both_passive_listener_sets(self):
        records = [{'name': f'case-{index}', 'value': [index, False]} for index in range(73)]
        records += [{'name': name, 'error': None, 'svg': '<svg/>', 'wrappers': [{}]} for name in EVENTS]
        source = {'records': records, 'passiveHost': {'records': [{'name': name, 'listeners': [{'type': event} for event in events]} for name, events in EVENTS.items()]}}
        old, native = copy.deepcopy(source), copy.deepcopy(source)
        lock = {'actionRows': list(EVENTS)}
        unchanged, matches, failures = observations(source, native, old, lock)
        self.assertEqual((len(unchanged), len(matches), failures), (73, 75, []))
        for index in range(75):
            changed = copy.deepcopy(native)
            changed['records'][index]['extra'] = True
            self.assertTrue(observations(source, changed, old, lock)[2])
        for name in EVENTS:
            changed = copy.deepcopy(source)
            next(row for row in changed['passiveHost']['records'] if row['name'] == name)['listeners'].clear()
            self.assertTrue(observations(changed, native, old, lock)[2])
        changed = copy.deepcopy(source)
        changed['records'][0]['value'][0] += 1e-12
        self.assertTrue(any(row['kind'] == 'old-source-preservation' for row in observations(changed, native, old, lock)[2]))


if __name__ == '__main__':
    unittest.main()
