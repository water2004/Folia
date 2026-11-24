"""Complete ordered JSON diagnostics for source record validators."""
from .common import MISSING, js_type, keys, same_scalar, shown


def differences(expected, actual, at='$', *, host=False):
    result = []
    left_name, right_name = ('expected', 'actual') if host else ('source', 'native')

    def report(path, left, right):
        result.append({'path': path, left_name: shown(left), right_name: shown(right)})

    def visit(left, right, path):
        if same_scalar(left, right):
            return
        if left is None or right is None or not isinstance(left, (dict, list)) or not isinstance(right, (dict, list)):
            report(path, left, right)
            return
        if isinstance(left, list) != isinstance(right, list):
            if host:
                report(path, left, right)
            else:
                result.append({'path': path, 'sourceType': 'array' if isinstance(left, list) else js_type(left),
                               'nativeType': 'array' if isinstance(right, list) else js_type(right)})
            return
        if isinstance(left, list) and not host:
            if len(left) != len(right):
                report(path + '.length', len(left), len(right))
            for index in range(min(len(left), len(right))):
                visit(left[index], right[index], f'{path}[{index}]')
            return
        left_map = {str(index): item for index, item in enumerate(left)} if isinstance(left, list) else left
        right_map = {str(index): item for index, item in enumerate(right)} if isinstance(right, list) else right
        left_keys, right_keys = keys(left_map), keys(right_map)
        if left_keys != right_keys:
            report(path + '.[key-order]', left_keys, right_keys)
        for name in dict.fromkeys(left_keys + right_keys):
            if name not in left_map:
                report(path + '.' + name, '<absent>' if host else {'$': 'missing'}, right_map[name])
            elif name not in right_map:
                if host:
                    report(path + '.' + name, left_map[name], '<absent>')
            else:
                visit(left_map[name], right_map[name], f'{path}[{name}]' if isinstance(left, list) else path + '.' + name)
    visit(expected, actual, at)
    return result
