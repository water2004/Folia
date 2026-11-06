export const cases = [
  ['domains', 'cynefin-beta\ncomplex\n  "Emergent practice"\n  "Probe-sense-respond"\ncomplicated\n  "Expert analysis"\nclear\n  "Best practice"\nchaotic\n  "Novel practice"\nconfusion\n  "Disorder"\n', null],
  ['transition', 'cynefin-beta:\ntitle Team practices\naccTitle: Cynefin model\naccDescr: A five domain framework\ncomplex\n  "Retrospectives"\ncomplicated\n  "Code review"\nclear\n  "Deployment checklist"\nchaotic\n  "Incident response"\nconfusion\n  "New initiative"\ncomplex --> complicated : "Pattern emerges"\ncomplicated --> clear : "Codified"\nchaotic --> complex\nconfusion --> chaotic : "Stabilized"\n', null],
  ['empty-domain', 'cynefin-beta\ncomplex\nclear\n', null],
  ['self-loop-filter', 'cynefin-beta\ncomplex\n  "A"\ncomplex --> complex : "Self loop"\n', null],
  ['unknown-domain', 'cynefin-beta\nunknown\n', 'syntax error'],
  ['orphan-label', 'cynefin-beta\n"No domain"\n', 'syntax error'],
];
