const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function loadUI(file, response) {
  const source = fs.readFileSync(path.join(__dirname, '..', file), 'utf8');
  const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
  const nodes = new Map();
  const context = vm.createContext({
    fetch: async () => {
      if (response instanceof Error) throw response;
      return { ok: response.status === 200, status: response.status,
               text: async () => response.text };
    },
    document: { getElementById(id) {
      if (!nodes.has(id)) nodes.set(id, { className: '', small: {},
        querySelector() { return this.small; } });
      return nodes.get(id);
    } },
  });
  vm.runInContext(script, context);
  context.logs = [];
  vm.runInContext('addLog = (msg) => logs.push(msg)', context);
  return { context, nodes };
}

for (const file of ['src/main.cpp', 'attacker.ino']) {
  for (const type of ['brake', 'hijack', 'flood']) {
    for (const counts of ['driver accepted: 1; driver errors: 0',
                          'driver accepted: 0; driver errors: 1',
                          'driver accepted: 7; driver errors: 13']) {
      test(`${file}: ${type} displays driver result without inventing mitigation`, async () => {
        const text = `${counts}. Delivery and FPGA blocking are UNCONFIRMED.`;
        const { context, nodes } = loadUI(file, { status: 200, text });
        if (file.endsWith('.cpp')) await context.inject(type, 'ecu-brakes');
        else await context.launchAttack(type);
        assert.equal(context.logs.at(-1), text);
        assert.doesNotMatch(context.logs.join(' '), /DESTROYED|SUCCESSFULLY|COMPROMISED|MITIGATION/);
        if (nodes.has('ecu-brakes'))
          assert.equal(nodes.get('ecu-brakes').small.textContent, 'DELIVERY UNCONFIRMED');
      });
    }
  }
  test(`${file}: HTTP errors are not displayed as success`, async () => {
    const { context } = loadUI(file, { status: 400, text: 'Unknown payload type' });
    if (file.endsWith('.cpp')) await context.inject('bad', 'ecu-brakes');
    else await context.launchAttack('bad');
    assert.match(context.logs.at(-1), /400.*Unknown payload type/);
  });
  test(`${file}: connection failure does not imply a block`, async () => {
    const { context } = loadUI(file, new Error('offline'));
    if (file.endsWith('.cpp')) await context.inject('brake', 'ecu-brakes');
    else await context.launchAttack('brake');
    assert.match(context.logs.at(-1), /ERR_CONN|Connection error/);
  });
}
