"""Dispatch family-specific acceptance policies without loading a JS runtime."""

from . import family_flowchart, family_graphs, family_labels, family_simple, family_structures


POLICIES = (family_simple, family_graphs, family_labels, family_structures, family_flowchart)
FAMILIES = set().union(*(module.FAMILIES for module in POLICIES))


def compare_family(family, actual, expected, check, oracle=False):
    for module in POLICIES:
        if family in module.FAMILIES:
            return module.compare_family(family, actual, expected, check, oracle)
    raise ValueError("Unknown diagram family: " + family)
